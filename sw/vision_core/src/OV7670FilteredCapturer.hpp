#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include "FilteredCapturer.hpp"

namespace FPGAlix {

/* FilteredCapturer for the OV7670 sensor via Altera mSGDMA V4L2 driver.
   The camera delivers 640×480 @ 30 fps raw Bayer (CV_8UC1); the filter
   pipeline (demosaicing etc.) converts to the application output format.
   Use WIDTH / HEIGHT / FPS for FrameBuffer size and Streamer configuration;
   the FrameBuffer format must match the application output format, not CV_8UC1. */
class OV7670FilteredCapturer : public FilteredCapturer {
public:
    static constexpr int WIDTH  = 640;
    static constexpr int HEIGHT = 480;
    static constexpr int FPS    = 30;

    OV7670FilteredCapturer(const std::string &device, FrameBuffer &outputBuffer);

    cv::VideoCapture& openDevice() override;

private:
    std::string      m_device;
    cv::VideoCapture m_cap;
};

} // namespace FPGAlix
