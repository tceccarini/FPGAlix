#include "WebCamFilteredCapturer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

WebCamFilteredCapturer::WebCamFilteredCapturer(const std::string &device, FrameBuffer &buffer)
    : FilteredCapturer(buffer), m_device(device) {}

void WebCamFilteredCapturer::probeDevice(const std::string &device,
                                         cv::Size desiredSize, int desiredFps,
                                         cv::Size *outSize, int *outFps) {
    cv::VideoCapture cap(device, cv::CAP_V4L2);
    if (!cap.isOpened())
        throw ExceptionDeviceError("WebCamFilteredCapturer::probeDevice: failed to open " + device);
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  desiredSize.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, desiredSize.height);
    cap.set(cv::CAP_PROP_FPS,          desiredFps);
    *outSize = cv::Size(static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
                        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    int actual = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
    *outFps = (actual > 0) ? actual : desiredFps;
    cap.release();
}

void WebCamFilteredCapturer::setDevice(cv::Size resolution, int fps) {
    m_resolution = resolution;
    m_fps        = fps;
}

cv::VideoCapture& WebCamFilteredCapturer::openDevice() {
    m_cap.open(m_device, cv::CAP_V4L2);
    if (!m_cap.isOpened())
        throw ExceptionDeviceError("WebCamFilteredCapturer::openDevice: failed to open " + m_device);
    m_cap.set(cv::CAP_PROP_CONVERT_RGB,  1);   // mandatory: deliver BGR frames
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  m_resolution.width);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_resolution.height);
    m_cap.set(cv::CAP_PROP_FPS,          m_fps);

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
