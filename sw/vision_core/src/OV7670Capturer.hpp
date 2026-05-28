#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include "Capturer.hpp"

namespace FPGAlix {

/* Capturer for the OV7670 sensor via Altera mSGDMA V4L2 driver.
   Delivers 640×480 @ 30 fps raw Bayer (SRGGB8) into the output FrameBuffer. */
class OV7670Capturer : public Capturer {
public:
    static constexpr int WIDTH  = 640;
    static constexpr int HEIGHT = 480;
    static constexpr int FPS    = 30;

    OV7670Capturer(const std::string &device, FrameBuffer &outputBuffer);

    cv::VideoCapture& openDevice() override;
    void setInputBufferLength(int n = 4);

private:
    std::string      m_device;
    int              m_inputBufferLength{4};
    cv::VideoCapture m_cap; /* unused in V4L2 path, returned as dummy by openDevice() */
};

} // namespace FPGAlix
