#pragma once
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <atomic>
#include <thread>
#include "FrameBuffer.hpp"

namespace FPGAlix {

/* Reads frames from a FrameBuffer and streams them via RTSP using GStreamer.
   Does no processing — pure transport layer.

   Typical usage:
     streamer.start();   // spawns GMainLoop thread and transmission thread
     ...
     streamer.stop();    // unblocks pop(), joins threads cleanly */
class Streamer {
public:
    enum class Encoding { UNCOMPRESSED, MJPEG, H264 };

    /* buffer:   source of frames to transmit (shared with Capturer)
       width/height: frame dimensions, must match FrameBuffer
       fps:      stream framerate, defaults to 30
       encoding: compression algorithm
       format:   OpenCV pixel type of the frames (CV_8UC1 = GRAY8, CV_8UC3 = BGR)

       NOTE — UNCOMPRESSED + CV_8UC1: RFC 4175 (rtpvrawpay) does not define a
       GRAY8 payload type. The pipeline internally converts GRAY8→RGB via
       videoconvert before packetising (I420 is avoided because videoconvert
       sets U/V to 0 instead of 128, producing a green tint). An intermediate
       colorspace conversion therefore still occurs even in "uncompressed" mode. */
    explicit Streamer(FrameBuffer &buffer, int width, int height, int fps = 30,
                      Encoding encoding = Encoding::MJPEG,
                      int format = CV_8UC3);
    virtual ~Streamer();

    /* Starts the GMainLoop thread and the transmission thread */
    void start();

    /* Signals FrameBuffer to unblock pop(), stops both threads and joins them */
    void stop();

private:
    /* Called once when the first client connects — configures appsrc caps
       (format, resolution, framerate). Receives Streamer* via gpointer data. */
    static void onMediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer data);

    /* GStreamer destroy_notify — called when GStreamer is done transmitting a buffer.
       Calls frame->clearBusy() so the pool slot can be reused by borrow(). */
    static void onFrameRelease(gpointer data);

    /* Throws ExceptionInvalidFormat if the frame mat does not match the
       expected dimensions, pixel type, or is not contiguous in memory. */
    void validateFrame(const Frame &frame) const;

    /* Transmission thread body: blocks on pop(), wraps the frame in a GstBuffer
       (zero copy), pushes to appsrc, exits on ExceptionQueueEmpty. */
    void txThread();

    FrameBuffer        &m_buffer;        /* shared with Capturer — not owned */
    int                 m_width;
    int                 m_height;
    int                 m_fps;
    Encoding            m_encoding;
    int                 m_format;
    GstRTSPServer      *m_server{nullptr};
    GstAppSrc          *m_appsrc{nullptr}; /* set by onMediaConfigure on first client */
    GMainLoop          *m_loop{nullptr};
    std::atomic<bool>   m_running{false};
    std::thread         m_loopThread;    /* runs g_main_loop_run */
    std::thread         m_txThread;      /* runs txThread */
    GstClockTime        m_timestamp{0};  /* monotonic PTS counter */
};

} // namespace FPGAlix
