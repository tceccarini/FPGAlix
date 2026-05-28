#include "OV7670Capturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

OV7670Capturer::OV7670Capturer(const std::string &device, FrameBuffer &outputBuffer)
    : Capturer(outputBuffer), m_device(device) {}

void OV7670Capturer::setInputBufferLength(int n) { m_inputBufferLength = n; }

cv::VideoCapture& OV7670Capturer::openDevice() {
    Capturer::openDevice(m_device, WIDTH, HEIGHT, FPS, V4L2_PIX_FMT_SRGGB8, m_inputBufferLength);
    return m_cap;
}

} // namespace FPGAlix
