#include "WebCamCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

WebCamCapturer::WebCamCapturer(const std::string &device, FrameBuffer &outputBuffer, int fps)
    : Capturer(outputBuffer), m_device(device), m_fps(fps), m_outputBuffer(outputBuffer) {}

void WebCamCapturer::setInputBufferLength(int n) { m_inputBufferLength = n; }

cv::VideoCapture& WebCamCapturer::openDevice() {
    cv::Size res = m_outputBuffer.getFrame(0).mat().size();
    Capturer::openDevice(m_device, res.width, res.height, m_fps, V4L2_PIX_FMT_YUYV, m_inputBufferLength);
    return m_cap;
}

} // namespace FPGAlix
