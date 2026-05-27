#pragma once
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include "FrameBuffer.hpp"

namespace FPGAlix {

/* Reads frames from a FrameBuffer and streams them via RTSP using GStreamer.
   Does no processing — pure transport layer.

   Valid encoding/format combinations:
     UNCOMPRESSED + BGR8  — rtpvrawpay, BGR passed directly (no conversion)
     MJPEG        + BGR8  — cv::imencode BGR→YCbCr, rtpjpegpay
     H264         + BGR8  — videoconvert BGR→I420, openh264enc
     H264         + GRAY8 — videoconvert GRAY→I420, openh264enc
   All other combinations throw ExceptionInvalidFormat in setInputBuffer().

   Typical usage:
     Streamer streamer(fps, encoding);
     streamer.setInputBuffer(inputBuffer);  // validates format; must be called before start()
     streamer.start();
     ...
     streamer.stop(); */
class Streamer {
public:
    enum class Encoding { UNCOMPRESSED, MJPEG, H264 };

    /* fps:      stream framerate
       encoding: compression algorithm
       Format and dimensions are deduced from the FrameBuffer in setInputBuffer(). */
    explicit Streamer(int fps, Encoding encoding);
    virtual ~Streamer();

    /* Provides the FrameBuffer to read frames from.
       Must be called before start(). The buffer is not owned by the Streamer. */
    void setInputBuffer(FrameBuffer &inputBuffer);

    /* Returns the CV pixel types accepted by the current encoding,
       ordered by ascending computational cost (index 0 = least effort).
       UNCOMPRESSED → { CV_8UC3 }
       MJPEG        → { CV_8UC3 }
       H264         → { CV_8UC1, CV_8UC3 }  (GRAY8 skips chroma processing) */
    std::vector<int> getAvailableInputFormats() const;

    /* Starts the GMainLoop thread and the transmission thread */
    void start();

    /* Signals FrameBuffer to unblock pop(), stops both threads and joins them */
    void stop();

    /* Sets the JPEG quality for MJPEG encoding [1-100]. Default: 95.
       Must be called before start(). Has no effect for other encodings. */
    void setMjpegQuality(int quality);

private:
    static void onMediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer data);
    static void onClientConnected(GstRTSPServer *server, GstRTSPClient *client, gpointer data);
    static void onClientClosed(GstRTSPClient *client, gpointer data);
    void txThread();

    FrameBuffer        *m_inputBuffer{nullptr}; /* set via setInputBuffer — not owned */
    int                 m_width;
    int                 m_height;
    int                 m_fps;
    Encoding            m_encoding;
    int                 m_format;
    int                 m_jpegQuality{60}; /* default quality for MJPEG encoding */
    GstRTSPServer      *m_server{nullptr};
    GstAppSrc          *m_appsrc{nullptr}; /* guarded by m_appsrcMtx */
    std::mutex          m_appsrcMtx;
    std::atomic<int>    m_clientCount{0};
    GMainLoop          *m_loop{nullptr};
    std::atomic<bool>   m_running{false};
    std::thread         m_loopThread;
    std::thread         m_txThread;
    GstClockTime        m_timestamp{0};
};

} // namespace FPGAlix
