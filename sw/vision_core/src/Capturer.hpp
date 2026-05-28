#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include "FrameBuffer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

/* Base class for frame capture.

   Typical usage:
     capturer.start();   // calls openDevice(), spawns capture thread
     ...
     capturer.stop();    // signals thread to exit, releases device */
class Capturer {
public:
    explicit Capturer(FrameBuffer &outputBuffer);
    virtual ~Capturer();

    /* Calls openDevice(), then spawns the capture thread.
       Throws ExceptionDeviceError if openDevice() returns an unopened capture. */
    void start();

    /* Signals the capture thread to stop, joins it, releases the device. */
    void stop();

    /* Opens and configures the capture source. Must be called before start().
       Subclasses override this to set up their specific device. */
    virtual cv::VideoCapture& openDevice();

    /* Opens a V4L2 device, verifies the driver accepted exactly the requested specs,
       allocates mmap buffers and queues them to the driver.
       Throws ExceptionDeviceError if any step fails or the driver negotiates different values. */
    void openDevice(const std::string &device, int width, int height,
                    int fps, uint32_t pixelFormat, int numBuffers = 4);

    /* Skip n frames between each processed frame (0 = process all).
       E.g. n=1 → process 1, skip 1, ... → effective fps halved. */
    void setDecimation(int n = 0);

protected:
    bool isRunning() const { return m_running.load(); }

private:
    struct MmapBuffer { void *start = nullptr; size_t length = 0; };

    void captureThread();

    FrameBuffer             &m_outputBuffer;
    cv::VideoCapture        *m_capPtr{nullptr};      /* points to the subclass-owned VideoCapture */
    int                      m_v4l2DeviceDescriptor{-1}; /* V4L2 device file descriptor */
    std::vector<MmapBuffer>  m_inputBuffer;
    int                      m_captureWidth{0};
    int                      m_captureHeight{0};
    uint32_t                 m_capturePixelFormat{0};
    std::atomic<bool>        m_running{false};
    std::thread              m_processThread;
    int                      m_decimation{0};
    int                      m_skipCount{0};
};

} // namespace FPGAlix
