#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include "Capturer.hpp"

namespace FPGAlix {

/* Capturer for a standard webcam opened by device path. */
class WebCamCapturer : public Capturer {
public:
    WebCamCapturer(const std::string &device, FrameBuffer &outputBuffer, int fps);

    cv::VideoCapture& openDevice() override;
    void setInputBufferLength(int n = 4);

private:
    std::string      m_device;
    int              m_fps;
    int              m_inputBufferLength{4};
    FrameBuffer     &m_outputBuffer;
    cv::VideoCapture m_cap; /* unused in V4L2 path, returned as dummy by openDevice() */
};

} // namespace FPGAlix
