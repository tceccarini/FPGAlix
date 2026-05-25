#include "WebCamFilteredCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

WebCamFilteredCapturer::WebCamFilteredCapturer(const std::string &device, FrameBuffer &buffer)
    : FilteredCapturer(buffer), m_device(device) {}

cv::Size WebCamFilteredCapturer::probeSize(const std::string &device, cv::Size desired) {
    cv::VideoCapture cap(device);
    if (!cap.isOpened())
        throw ExceptionDeviceError("WebCamFilteredCapturer::probeSize: failed to open " + device);
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  desired.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, desired.height);
    cv::Size actual(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
                    static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    cap.release();
    return actual;
}

void WebCamFilteredCapturer::setResolution(cv::Size size) {
    m_resolution = size;
}

cv::VideoCapture& WebCamFilteredCapturer::openDevice() {
    m_cap.open(m_device);
    if (!m_cap.isOpened())
        throw ExceptionDeviceError("WebCamFilteredCapturer::openDevice: failed to open " + m_device);
    m_cap.set(cv::CAP_PROP_CONVERT_RGB,  1);   // mandatory: deliver BGR frames
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  m_resolution.width);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_resolution.height);
    m_cap.set(cv::CAP_PROP_FPS,          30);

    cv::Size actual(static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH)),
                    static_cast<int>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    if (actual != m_resolution)
        throw ExceptionDeviceError("WebCamFilteredCapturer::openDevice: requested " +
            std::to_string(m_resolution.width) + "x" + std::to_string(m_resolution.height) +
            " but device gave " +
            std::to_string(actual.width) + "x" + std::to_string(actual.height));

    return m_cap;
}

} // namespace FPGAlix
