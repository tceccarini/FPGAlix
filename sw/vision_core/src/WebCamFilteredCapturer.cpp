#include "WebCamFilteredCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

WebCamFilteredCapturer::WebCamFilteredCapturer(const std::string &device, FrameBuffer &outputBuffer, int fps)
    : FilteredCapturer(outputBuffer), m_device(device), m_fps(fps), m_outputBuffer(outputBuffer) {}

cv::VideoCapture& WebCamFilteredCapturer::openDevice() {
    cv::Size res = m_outputBuffer.getFrame(0).mat().size();
    Capturer::openDevice(m_device, res.width, res.height, m_fps, V4L2_PIX_FMT_YUYV);
    return m_cap;
}

cv::Mat& WebCamFilteredCapturer::preFilter(cv::Mat &mat) {
    cv::cvtColor(mat, m_bgrFrame, cv::COLOR_YUV2BGR_YUYV);
    return m_bgrFrame;
}

} // namespace FPGAlix
