#pragma once
#include <atomic>
#include <thread>
#include <opencv2/opencv.hpp>
#include "FrameBuffer.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

/* Abstract base class for frame capture. Subclasses must implement openDevice()
   and may override process() if the raw frame needs conversion before pushing
   to the FrameBuffer.

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

    /* Opens and configures the capture source. Must return a reference to the
       cv::VideoCapture owned by the subclass. Called once by start(). */
    virtual cv::VideoCapture& openDevice() = 0;

    /* Converts one raw frame into the output pool frame.
       mat_in:  raw frame as returned by the VideoCapture
       mat_out: pre-allocated pool frame to write the result into */
    virtual void process(cv::Mat &mat_in, cv::Mat &mat_out) = 0;

protected:
    bool isRunning() const { return m_running.load(); }

private:
    void captureThread();

    FrameBuffer       &m_outputBuffer;
    cv::VideoCapture  *m_capPtr{nullptr}; /* points to the subclass-owned VideoCapture */
    std::atomic<bool>  m_running{false};
    std::thread        m_thread;
};

} // namespace FPGAlix
