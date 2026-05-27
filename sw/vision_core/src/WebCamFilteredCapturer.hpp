#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include "FilteredCapturer.hpp"

namespace FPGAlix {

/* Concrete FilteredCapturer for a standard webcam opened by device path.
   Provides openDevice(); the filter pipeline is managed by FilteredCapturer. */
class WebCamFilteredCapturer : public FilteredCapturer {
public:
    WebCamFilteredCapturer(const std::string &device, FrameBuffer &outputBuffer, int fps);

    cv::VideoCapture& openDevice() override;
    void setInputBufferLength(int n = 4);
    cv::Mat& preFilter(cv::Mat &mat) override;

private:
    std::string      m_device;
    int              m_fps;
    int              m_inputBufferLength{4};
    FrameBuffer     &m_outputBuffer;
    cv::VideoCapture m_cap; /* unused in V4L2 path, returned as dummy by openDevice() */
    cv::Mat          m_bgrFrame;
};

} // namespace FPGAlix
