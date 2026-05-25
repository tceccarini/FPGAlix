#include "Streamer.hpp"
#include "exception/Exceptions.hpp"
#include <opencv2/opencv.hpp>
#include <vector>

namespace FPGAlix {

Streamer::Streamer(FrameBuffer &buffer, int width, int height, int fps, Encoding encoding, int format)
    : m_buffer(buffer), m_width(width), m_height(height), m_fps(fps), m_encoding(encoding), m_format(format) {
    if (format != CV_8UC1 && format != CV_8UC3)
        throw ExceptionInvalidFormat("Streamer: unsupported format, expected CV_8UC1 (GRAY8) or CV_8UC3 (BGR)");
}

Streamer::~Streamer() {
    stop();
}

void Streamer::validateFrame(const Frame &frame) const {
    const cv::Mat &mat = frame.mat();

    if (mat.empty())
        throw ExceptionInvalidFormat("Streamer: frame mat is empty");
    if (mat.cols != m_width || mat.rows != m_height)
        throw ExceptionInvalidFormat("Streamer: frame size does not match expected dimensions");
    if (mat.type() != m_format)
        throw ExceptionInvalidFormat("Streamer: frame pixel type does not match expected format");
    if (!mat.isContinuous())
        throw ExceptionInvalidFormat("Streamer: frame mat is not contiguous — zero-copy push not safe");
}

void Streamer::onMediaConfigure(GstRTSPMediaFactory *, GstRTSPMedia *media, gpointer data) {
    Streamer *self = static_cast<Streamer*>(data);
    GstElement *pipeline = gst_rtsp_media_get_element(media);
    GstElement *src = gst_bin_get_by_name_recurse_up(GST_BIN(pipeline), "src");
    if (!src) {
        gst_object_unref(pipeline);
        return;
    }

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

    self->m_appsrc = GST_APP_SRC(src);
    gst_object_unref(pipeline);
}

void Streamer::onFrameRelease(gpointer data) {
    static_cast<Frame*>(data)->clearBusy();
}

void Streamer::txThread() {
    const GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, m_fps);

    while (m_running) {
        Frame *frame = nullptr;
        try {
            frame = m_buffer.pop();
        }
        catch (const ExceptionQueueEmpty &) {
            break; /* forceNotify() called — clean exit */
        }

        if (!m_appsrc) { m_buffer.giveBack(frame); continue; }

        try {
            validateFrame(*frame);
        }
        catch (const ExceptionInvalidFormat &) {
            m_buffer.giveBack(frame);
            continue;
        }

        GstBuffer *buf;
        if (m_encoding == Encoding::MJPEG) {
            auto *jpegBuf = new std::vector<uchar>();
            cv::imencode(".jpg", frame->mat(), *jpegBuf);
            m_buffer.giveBack(frame); /* frame copiato nel JPEG — libero subito */
            buf = gst_buffer_new_wrapped_full(
                GST_MEMORY_FLAG_READONLY,
                jpegBuf->data(), jpegBuf->size(), 0, jpegBuf->size(),
                jpegBuf,
                [](gpointer d) { delete static_cast<std::vector<uchar>*>(d); }
            );
        } else {
            const int frameSize = frame->mat().total() * frame->mat().elemSize();
            buf = gst_buffer_new_wrapped_full(
                GST_MEMORY_FLAG_READONLY,
                frame->mat().data,
                frameSize, 0, frameSize,
                frame, onFrameRelease
            );
        }

        GST_BUFFER_PTS(buf)      = m_timestamp;
        GST_BUFFER_DURATION(buf) = duration;
        m_timestamp += duration;

        gst_app_src_push_buffer(m_appsrc, buf);
    }
}

void Streamer::start() {
    gst_init(NULL, NULL);
    m_running = true;
    m_loop    = g_main_loop_new(NULL, FALSE);

    m_server = gst_rtsp_server_new();

    GstRTSPMountPoints  *mounts  = gst_rtsp_server_get_mount_points(m_server);
    GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();

    const char *pipeline;
    switch (m_encoding) {
        case Encoding::UNCOMPRESSED:
            if (m_format == CV_8UC1)
                pipeline = "( appsrc name=src ! videoconvert ! video/x-raw,format=RGB ! rtpvrawpay name=pay0 pt=96 )";
            else
                pipeline = "( appsrc name=src ! videoconvert ! video/x-raw,format=RGB ! rtpvrawpay name=pay0 pt=96 )";
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
    gst_rtsp_mount_points_add_factory(mounts, "/stream", factory);
    g_object_unref(mounts);

    gst_rtsp_server_attach(m_server, NULL);

    m_loopThread = std::thread([this] { g_main_loop_run(m_loop); });
    m_txThread   = std::thread([this] { txThread(); });
}

void Streamer::stop() {
    m_running = false;
    m_buffer.forceNotify();
    if (m_loop) g_main_loop_quit(m_loop);
    if (m_loopThread.joinable()) m_loopThread.join();
    if (m_txThread.joinable())   m_txThread.join();
}

} // namespace FPGAlix
