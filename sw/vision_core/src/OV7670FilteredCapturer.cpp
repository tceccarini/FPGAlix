#include "OV7670FilteredCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

OV7670FilteredCapturer::OV7670FilteredCapturer(const std::string &device, FrameBuffer &outputBuffer)
    : FilteredCapturer(outputBuffer), m_device(device) {}

void OV7670FilteredCapturer::setInputBufferLength(int n) { m_inputBufferLength = n; }

cv::VideoCapture& OV7670FilteredCapturer::openDevice() {
    Capturer::openDevice(m_device, WIDTH, HEIGHT, FPS, V4L2_PIX_FMT_SRGGB8, m_inputBufferLength);
    return m_cap;
}


} // namespace FPGAlix
