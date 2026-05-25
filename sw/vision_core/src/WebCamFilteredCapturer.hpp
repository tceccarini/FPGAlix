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

    /* Opens device, requests desired size, reads back what the driver
       actually negotiated, then closes. Use the returned size to construct
       FrameBuffer and Streamer before calling start(). */
    static cv::Size probeSize(const std::string &device, cv::Size desired);

    /* Sets the resolution that openDevice() will request and verify.
       Must be called before start(). Use probeSize() to find a size the
       device actually supports. */
    void setResolution(cv::Size size);

    cv::VideoCapture& openDevice() override;

private:
    std::string      m_device;
    cv::Size         m_resolution{640, 480};
    cv::VideoCapture m_cap;
};

} // namespace FPGAlix
