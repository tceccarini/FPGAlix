#include "OV7670FilteredCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

OV7670FilteredCapturer::OV7670FilteredCapturer(const std::string &device, FrameBuffer &outputBuffer)
    : FilteredCapturer(outputBuffer), m_device(device) {}

cv::VideoCapture& OV7670FilteredCapturer::openDevice() {
    m_cap.open(m_device, cv::CAP_V4L2);
    if (!m_cap.isOpened())
        throw ExceptionDeviceError("OV7670FilteredCapturer: failed to open " + m_device);

    m_cap.set(cv::CAP_PROP_CONVERT_RGB, 0);     // raw Bayer, no conversion
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  WIDTH);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, HEIGHT);
    m_cap.set(cv::CAP_PROP_FPS,          FPS);

    return m_cap;
}

} // namespace FPGAlix
