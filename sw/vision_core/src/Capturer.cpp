#include "Capturer.hpp"
#include "Utils.hpp"

namespace FPGAlix {

Capturer::Capturer(FrameBuffer &outputBuffer)
    : m_outputBuffer(outputBuffer) {
}

Capturer::~Capturer() {
    stop();
}

void Capturer::start() {
    m_capPtr = &openDevice();
    if (!m_capPtr->isOpened())
        throw ExceptionDeviceError("Capturer: openDevice() did not open a device");

    m_running = true;
    m_thread  = std::thread([this] { captureThread(); });
}

void Capturer::stop() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
    if (m_capPtr)
        m_capPtr->release();
}

void Capturer::captureThread() {
    cv::Mat raw;

    while (m_running) {
        Frame *frame = nullptr;
        try {
            frame = m_outputBuffer.borrow();
        }
        catch (const ExceptionPoolExhausted &) {
            rprint("Capturer: pool exhausted, skipping frame\n");
            continue;
        }

        if (!m_capPtr->read(raw)) {
            m_outputBuffer.giveBack(frame);
            rprint("Capturer: read failed, exiting capture thread\n");
            break;
        }

        process(raw, frame->mat());

        try {
            m_outputBuffer.push(frame);
        }
        catch (const ExceptionQueueFull &) {
            rprint("Capturer: queue full, dropping frame\n");
            m_outputBuffer.giveBack(frame);
        }
    }
}

} // namespace FPGAlix
