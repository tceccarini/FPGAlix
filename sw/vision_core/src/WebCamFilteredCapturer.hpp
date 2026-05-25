#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include "FilteredCapturer.hpp"

namespace FPGAlix {

/* Concrete FilteredCapturer for a standard webcam opened by device path.
   Provides openDevice(); the filter pipeline is managed by FilteredCapturer. */
class WebCamFilteredCapturer : public FilteredCapturer {
public:
    /* device: V4L2 device path (e.g. "/dev/video0").
       buffer: FrameBuffer to push captured frames into. */
    WebCamFilteredCapturer(const std::string &device, FrameBuffer &buffer);

    /* Opens device once, requests desiredSize and desiredFps, writes the
       values actually negotiated by the driver into *outSize and *outFps,
       then closes. Use the results to construct FrameBuffer and Streamer
       before calling start(). */
    static void probeDevice(const std::string &device,
                            cv::Size desiredSize, int desiredFps,
                            cv::Size *outSize, int *outFps);

    /* Sets the resolution and framerate that openDevice() will request.
       Must be called before start(). Use probeDevice() to find values the
       device actually supports. */
    void setDevice(cv::Size resolution, int fps);

    cv::VideoCapture& openDevice() override;

private:
    std::string      m_device;
    cv::Size         m_resolution{640, 480};
    int              m_fps{30};
    cv::VideoCapture m_cap;
};

} // namespace FPGAlix
