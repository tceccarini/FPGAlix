#include "Streamer.hpp"
#include "exception/Exceptions.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <vector>

namespace FPGAlix {

Streamer::Streamer(int fps, Encoding encoding)
    : m_fps(fps), m_encoding(encoding) {
}

Streamer::~Streamer() {
    stop();
}

void Streamer::setInputBuffer(FrameBuffer &inputBuffer) {
    const cv::Mat &mat = inputBuffer.getFrame(0).mat();
    m_width  = mat.cols;
    m_height = mat.rows;
    m_format = mat.type();

    const std::vector<int> accepted = getAvailableInputFormats();
    if (std::find(accepted.begin(), accepted.end(), m_format) == accepted.end())
        throw ExceptionInvalidFormat("Streamer: buffer format not accepted by the selected encoding");

    for (int i = 0; i < inputBuffer.getSize(); ++i)
        if (!inputBuffer.getFrame(i).mat().isContinuous())
            throw ExceptionInvalidFormat("Streamer: frame mat is not contiguous — zero-copy push not safe");

    m_inputBuffer = &inputBuffer;
}

std::vector<int> Streamer::getAvailableInputFormats() const {
    switch (m_encoding) {
        case Encoding::UNCOMPRESSED:
            return {CV_8UC3};
        case Encoding::MJPEG:
            return {CV_8UC3};
        case Encoding::H264:
            return {CV_8UC1, CV_8UC3};   // GRAY8 first: no chroma, less encoder effort
        default:
            throw ExceptionInvalidFormat("Streamer: unsupported encoding");
    }
}

void Streamer::onMediaConfigure(GstRTSPMediaFactory *, GstRTSPMedia *media, gpointer data) {
    Streamer *self = static_cast<Streamer*>(data);

    /* Keep the pipeline alive (PAUSED) when all clients disconnect so the next
       client can reconnect without the slow NULL→PLAYING rebuild cycle. */
    gst_rtsp_media_set_reusable(media, TRUE);

    GstElement *pipeline = gst_rtsp_media_get_element(media);
    GstElement *src = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline), "src");
    gst_object_unref(pipeline);
    if (!src)
        return;

    GstCaps *caps;
    if (self->m_encoding == Encoding::MJPEG) {
        caps = gst_caps_new_simple("image/jpeg",
            "width",     G_TYPE_INT,        self->m_width,
            "height",    G_TYPE_INT,        self->m_height,
            "framerate", GST_TYPE_FRACTION, self->m_fps, 1,
            NULL);
    } else {
        const char *fmt = (self->m_format == CV_8UC1) ? "GRAY8" : "BGR";
        caps = gst_caps_new_simple("video/x-raw",
            "format",    G_TYPE_STRING,     fmt,
            "width",     G_TYPE_INT,        self->m_width,
            "height",    G_TYPE_INT,        self->m_height,
            "framerate", GST_TYPE_FRACTION, self->m_fps, 1,
            NULL);
    }
    g_object_set(src, "caps", caps, "format", GST_FORMAT_TIME, "is-live", TRUE, NULL);
    gst_caps_unref(caps);

    std::lock_guard<std::mutex> lk(self->m_appsrcMtx);
    if (self->m_appsrc)
        gst_object_unref(GST_OBJECT(self->m_appsrc));
    self->m_appsrc = GST_APP_SRC(src);
}

void Streamer::onClientConnected(GstRTSPServer *, GstRTSPClient *client, gpointer data) {
    Streamer *self = static_cast<Streamer*>(data);
    self->m_clientCount.fetch_add(1, std::memory_order_relaxed);
    g_signal_connect(client, "closed", G_CALLBACK(onClientClosed), data);
}

void Streamer::onClientClosed(GstRTSPClient *, gpointer data) {
    static_cast<Streamer*>(data)->m_clientCount.fetch_sub(1, std::memory_order_relaxed);
}

void Streamer::txThread() {
    const GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, m_fps);
    bool prevHadClients = false;

    while (m_running) {
        Frame *frame = nullptr;
        try {
            frame = m_inputBuffer->pop();
        }
        catch (const ExceptionQueueEmpty &) {
            break;
        }

        GstAppSrc *appsrc;
        {
            std::lock_guard<std::mutex> lk(m_appsrcMtx);
            appsrc = m_appsrc;
        }
        const bool hasClients = m_clientCount.load(std::memory_order_relaxed) > 0;
        if (!appsrc || !hasClients) {
            /* On the first frame after the last client disconnected, flush the
               GStreamer pipeline to discard queued buffers — zero-copy frames
               are returned to the pool, and MJPEG frames don't pile up causing
               latency spikes when a new client reconnects. */
            if (appsrc && prevHadClients) {
                gst_element_send_event(GST_ELEMENT(appsrc), gst_event_new_flush_start());
                gst_element_send_event(GST_ELEMENT(appsrc), gst_event_new_flush_stop(TRUE));
                m_timestamp = 0;
            }
            prevHadClients = false;
            m_inputBuffer->giveBack(frame);
            continue;
        }
        prevHadClients = true;

        GstBuffer *buf;
        if (m_encoding == Encoding::MJPEG) {
            /* MJPEG always encodes into a new buffer — copy is unavoidable */
            auto *outBuf = new std::vector<uchar>();
            cv::imencode(".jpg", frame->mat(), *outBuf);
            m_inputBuffer->giveBack(frame);
            buf = gst_buffer_new_wrapped_full(
                GST_MEMORY_FLAG_READONLY,
                outBuf->data(), outBuf->size(), 0, outBuf->size(),
                outBuf,
                [](gpointer d) { delete static_cast<std::vector<uchar>*>(d); }
            );
        } else {
            /* Zero-copy: wrap the frame mat directly; release back to pool
               only when GStreamer has finished reading (destroy callback). */
            using ReleaseCtx = std::pair<FrameBuffer*, Frame*>;
            const gsize frameSize = frame->mat().total() * frame->mat().elemSize();
            buf = gst_buffer_new_wrapped_full(
                GST_MEMORY_FLAG_READONLY,
                frame->mat().data, frameSize, 0, frameSize,
                new ReleaseCtx{m_inputBuffer, frame},
                [](gpointer d) {
                    auto *ctx = static_cast<ReleaseCtx*>(d);
                    ctx->first->giveBack(ctx->second);
                    delete ctx;
                }
            );
        }

        GST_BUFFER_PTS(buf)      = m_timestamp;
        GST_BUFFER_DURATION(buf) = duration;
        m_timestamp += duration;

        gst_app_src_push_buffer(appsrc, buf);
    }
}

void Streamer::start() {
    if (!m_inputBuffer)
        throw ExceptionInvalidFormat("Streamer: setInputBuffer() must be called before start()");

    gst_init(NULL, NULL);
    m_running = true;
    m_loop    = g_main_loop_new(NULL, FALSE);

    m_server = gst_rtsp_server_new();

    GstRTSPMountPoints  *mounts  = gst_rtsp_server_get_mount_points(m_server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    const char *pipeline;
    switch (m_encoding) {
        case Encoding::UNCOMPRESSED:
            pipeline = "( appsrc name=src ! rtpvrawpay name=pay0 pt=96 )";
            break;
        case Encoding::MJPEG:
            pipeline = "( appsrc name=src ! rtpjpegpay name=pay0 pt=26 )";
            break;
        case Encoding::H264:
            pipeline = "( appsrc name=src ! videoconvert ! openh264enc ! rtph264pay name=pay0 pt=96 )";
            break;
        default:
            throw ExceptionInvalidFormat("Streamer: unsupported encoding");
    }
    gst_rtsp_media_factory_set_launch(factory, pipeline);
    g_signal_connect(factory, "media-configure", G_CALLBACK(onMediaConfigure), this);
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect(m_server, "client-connected", G_CALLBACK(onClientConnected), this);
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(m_server, NULL);

    m_loopThread = std::thread([this] { g_main_loop_run(m_loop); });
    m_txThread   = std::thread([this] { txThread(); });
}

void Streamer::stop() {
    m_running = false;
    if (m_inputBuffer) m_inputBuffer->forceNotify();
    if (m_loop) g_main_loop_quit(m_loop);
    if (m_loopThread.joinable()) m_loopThread.join();
    if (m_txThread.joinable())   m_txThread.join();
    std::lock_guard<std::mutex> lk(m_appsrcMtx);
    if (m_appsrc) {
        gst_object_unref(GST_OBJECT(m_appsrc));
        m_appsrc = nullptr;
    }
}

} // namespace FPGAlix
