#include "Capturer.hpp"
#include "Utils.hpp"
#include <pthread.h>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <cerrno>
#include <cstring>

namespace FPGAlix {

Capturer::Capturer(FrameBuffer &outputBuffer)
    : m_outputBuffer(outputBuffer) {
}

Capturer::~Capturer() {
    stop();
}

void Capturer::start() {
    if (m_v4l2DeviceDescriptor < 0)
        throw ExceptionDeviceError("Capturer: device not open — call openDevice() before start()");

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_STREAMON, &type) < 0)
        throw ExceptionDeviceError("Capturer: VIDIOC_STREAMON failed: " + std::string(strerror(errno)));

    m_running = true;
    m_processThread = std::thread([this] {
        pthread_setname_np(pthread_self(), "capture");
        captureThread();
    });
}

void Capturer::stop() {
    m_running = false;
    if (m_processThread.joinable())
        m_processThread.join();

    if (m_v4l2DeviceDescriptor >= 0) {
        /* Drain buffers still owned by the driver before STREAMOFF.
           If we call STREAMOFF while a DMA transfer is in flight, the mSGDMA
           driver's dmaengine_terminate_sync races with the HW completion
           interrupt and hits BUG_ON(cookie < 1) in dma_cookie_complete. */
        {
            v4l2_buffer buf{};
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            for (size_t i = 0; i < m_inputBuffer.size(); ++i) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(m_v4l2DeviceDescriptor, &fds);
                timeval tv{0, 100000};
                if (select(m_v4l2DeviceDescriptor + 1, &fds, nullptr, nullptr, &tv) <= 0)
                    break;
                ioctl(m_v4l2DeviceDescriptor, VIDIOC_DQBUF, &buf);
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(m_v4l2DeviceDescriptor, VIDIOC_STREAMOFF, &type);

        for (auto &buf : m_inputBuffer)
            if (buf.start)
                munmap(buf.start, buf.length);
        m_inputBuffer.clear();

        close(m_v4l2DeviceDescriptor);
        m_v4l2DeviceDescriptor = -1;
    } else if (m_capPtr) {
        m_capPtr->release();
    }
}

cv::VideoCapture& Capturer::openDevice() {
    throw ExceptionDeviceError("Capturer: openDevice() not implemented — use the V4L2 overload");
}

void Capturer::openDevice(const std::string &device, int width, int height,
                          int fps, uint32_t pixelFormat, int numBuffers) {
    // --- Open device ---
    m_v4l2DeviceDescriptor = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (m_v4l2DeviceDescriptor < 0)
        throw ExceptionDeviceError("Capturer: cannot open " + device + ": " + strerror(errno));

    // --- 1. Check capabilities ---
    v4l2_capability cap{};
    if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_QUERYCAP, &cap) < 0)
        throw ExceptionDeviceError("Capturer: VIDIOC_QUERYCAP failed: " + std::string(strerror(errno)));
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        throw ExceptionDeviceError("Capturer: device does not support VIDEO_CAPTURE");
    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        throw ExceptionDeviceError("Capturer: device does not support STREAMING");

    // --- 2. Set format and verify the driver accepted exactly the requested specs ---
    v4l2_format fmt{};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixelFormat;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_S_FMT, &fmt) < 0)
        throw ExceptionDeviceError("Capturer: VIDIOC_S_FMT failed: " + std::string(strerror(errno)));

    if ((int)fmt.fmt.pix.width != width || (int)fmt.fmt.pix.height != height)
        throw ExceptionDeviceError("Capturer: driver negotiated a different resolution");
    if (fmt.fmt.pix.pixelformat != pixelFormat)
        throw ExceptionDeviceError("Capturer: driver negotiated a different pixel format");

    m_captureWidth       = width;
    m_captureHeight      = height;
    m_capturePixelFormat = pixelFormat;

    v4l2_streamparm parm{};
    parm.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_S_PARM, &parm) < 0)
        throw ExceptionDeviceError("Capturer: VIDIOC_S_PARM failed: " + std::string(strerror(errno)));

    double actualFps = (double)parm.parm.capture.timeperframe.denominator /
                       (double)parm.parm.capture.timeperframe.numerator;
    if (std::abs(actualFps - fps) > 0.5)
        throw ExceptionDeviceError("Capturer: driver negotiated a different fps");

    // --- 3. Allocate mmap buffers and queue them to the driver ---
    v4l2_requestbuffers req{};
    req.count  = numBuffers;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_REQBUFS, &req) < 0)
        throw ExceptionDeviceError("Capturer: VIDIOC_REQBUFS failed: " + std::string(strerror(errno)));

    m_inputBuffer.resize(req.count);
    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_QUERYBUF, &buf) < 0)
            throw ExceptionDeviceError("Capturer: VIDIOC_QUERYBUF failed: " + std::string(strerror(errno)));

        m_inputBuffer[i].length = buf.length;
        m_inputBuffer[i].start  = mmap(nullptr, buf.length,
                                       PROT_READ | PROT_WRITE, MAP_SHARED,
                                       m_v4l2DeviceDescriptor, buf.m.offset);
        if (m_inputBuffer[i].start == MAP_FAILED)
            throw ExceptionDeviceError("Capturer: mmap failed: " + std::string(strerror(errno)));
    }

    for (unsigned i = 0; i < m_inputBuffer.size(); ++i) {
        v4l2_buffer buf{};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_QBUF, &buf) < 0)
            throw ExceptionDeviceError("Capturer: VIDIOC_QBUF failed: " + std::string(strerror(errno)));
    }
}

cv::Mat& Capturer::preFilter(cv::Mat &mat) {
    return mat;
}

void Capturer::setDecimation(int n) { m_decimation = n; }

static cv::Mat wrapBuffer(void *ptr, int w, int h, uint32_t pixfmt, uint32_t bytesused) {
    switch (pixfmt) {
    case V4L2_PIX_FMT_YUYV:  return cv::Mat(h, w, CV_8UC2, ptr);
    case V4L2_PIX_FMT_BGR24:
    case V4L2_PIX_FMT_RGB24:  return cv::Mat(h, w, CV_8UC3, ptr);
    case V4L2_PIX_FMT_GREY:
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:  return cv::Mat(h, w, CV_8UC1, ptr);
    case V4L2_PIX_FMT_MJPEG: {
        cv::Mat encoded(1, bytesused, CV_8UC1, ptr);
        return cv::imdecode(encoded, cv::IMREAD_COLOR);
    }
    default:
        return cv::Mat(h, w, CV_8UC1, ptr);
    }
}

void Capturer::captureThread() {
    if (m_v4l2DeviceDescriptor >= 0) {
        while (m_running) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_v4l2DeviceDescriptor, &fds);
            timeval tv{2, 0};

            int ret = select(m_v4l2DeviceDescriptor + 1, &fds, nullptr, nullptr, &tv);
            if (ret < 0)  { rprint("Capturer: select error\n"); break; }
            if (ret == 0) { continue; } // timeout, check again m_running

            v4l2_buffer buf{};
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (ioctl(m_v4l2DeviceDescriptor, VIDIOC_DQBUF, &buf) < 0) {
                rprint("Capturer: VIDIOC_DQBUF failed\n");
                break;
            }

            if (m_skipCount < m_decimation) {
                ++m_skipCount;
                ioctl(m_v4l2DeviceDescriptor, VIDIOC_QBUF, &buf);
                continue;
            }
            m_skipCount = 0;

            cv::Mat raw = wrapBuffer(m_inputBuffer[buf.index].start,
                                     m_captureWidth, m_captureHeight,
                                     m_capturePixelFormat, buf.bytesused);

            Frame *frame = nullptr;
            try {
                frame = m_outputBuffer.borrow();
            } catch (const ExceptionPoolExhausted &) {
                rprint("Capturer: pool exhausted, skipping frame\n");
                ioctl(m_v4l2DeviceDescriptor, VIDIOC_QBUF, &buf);
                continue;
            }

            cv::Mat &filtered = preFilter(raw);
            process(filtered, frame->mat());

            try {
                m_outputBuffer.push(frame);
            } catch (const ExceptionQueueFull &) {
                rprint("Capturer: queue full, dropping frame\n");
                m_outputBuffer.giveBack(frame);
            }

            // Return buffer to the driver only after process() has finished reading it
            ioctl(m_v4l2DeviceDescriptor, VIDIOC_QBUF, &buf);
        }
    } else {
        cv::Mat raw;
        while (m_running) {
            Frame *frame = nullptr;
            try {
                frame = m_outputBuffer.borrow();
            } catch (const ExceptionPoolExhausted &) {
                rprint("Capturer: pool exhausted, skipping frame\n");
                continue;
            }

            if (!m_capPtr->read(raw)) {
                m_outputBuffer.giveBack(frame);
                rprint("Capturer: read failed, exiting capture thread\n");
                break;
            }

            cv::Mat &filtered = preFilter(raw);
            process(filtered, frame->mat());

            try {
                m_outputBuffer.push(frame);
            } catch (const ExceptionQueueFull &) {
                rprint("Capturer: queue full, dropping frame\n");
                m_outputBuffer.giveBack(frame);
            }
        }
    }
}

} // namespace FPGAlix
