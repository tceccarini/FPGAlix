#include "FrameBuffer.hpp"

namespace FPGAlix {

FrameBuffer::FrameBuffer(int width, int height, int type, int size) {
    m_size  = size;
    m_pool  = new Frame*[size];
    m_queue = new Frame*[size];
    for (int i = 0; i < size; i++) {
        m_pool[i]  = new Frame(width, height, type);
        m_queue[i] = nullptr;
    }
}

FrameBuffer::~FrameBuffer() {
    for (int i = 0; i < m_size; i++)
        delete m_pool[i];
    delete[] m_pool;
    delete[] m_queue;
}

Frame *FrameBuffer::borrow() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int i = 0; i < m_size; i++) {
        Frame *frame = m_pool[m_poolHead];
        m_poolHead = (m_poolHead + 1) % m_size;
        if (!frame->isBusy()) {
            frame->setBusy();
            return frame;
        }
    }
    throw ExceptionPoolExhausted();
}

void FrameBuffer::push(Frame *frame) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (isQueueFull())
            throw ExceptionQueueFull();
        m_queue[m_queueHead] = frame;
        m_queueHead = (m_queueHead + 1) % m_size;
        ++m_queueSize;
    }
    m_cv.notify_one();
}

Frame *FrameBuffer::pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !isQueueEmpty() || m_forceNotify; });
    if (m_forceNotify)
        throw ExceptionQueueEmpty();
    Frame *frame = m_queue[m_queueTail];
    m_queueTail = (m_queueTail + 1) % m_size;
    --m_queueSize;
    return frame;
}

void FrameBuffer::forceNotify() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_forceNotify = true;
    m_cv.notify_all();
}

void FrameBuffer::giveBack(Frame *frame) {
    frame->clearBusy();
}


} // namespace FPGAlix
