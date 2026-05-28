#include "Processor.hpp"
#include "Utils.hpp"
#include <pthread.h>

namespace FPGAlix {

Processor::Processor(FrameBuffer &inputBuffer, FrameBuffer &outputBuffer)
    : m_inputBuffer(inputBuffer),
      m_outputBuffer(outputBuffer) {}

Processor::~Processor() {
    stop();
}

void Processor::start() {
    m_running = true;
    m_thread = std::thread([this] {
        pthread_setname_np(pthread_self(), "processor");
        processingThread();
    });
}

void Processor::stop() {
    m_running = false;
    m_inputBuffer.forceNotify();
    if (m_thread.joinable())
        m_thread.join();
}

/* --- pipeline editing ----------------------------------------------------- */

void Processor::appendFilter(std::unique_ptr<Filter> filter) {
    m_pending.push_back(std::shared_ptr<Filter>(std::move(filter)));
}

void Processor::prependFilter(std::unique_ptr<Filter> filter) {
    insertFilter(0, std::move(filter));
}

void Processor::insertFilter(size_t pos, std::unique_ptr<Filter> filter) {
    if (pos > m_pending.size())
        throw ExceptionOutOfRange("Processor::insertFilter: pos out of range");
    m_pending.insert(m_pending.begin() + static_cast<std::ptrdiff_t>(pos),
                     std::shared_ptr<Filter>(std::move(filter)));
}

void Processor::removeFilter(uint32_t id) {
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if ((*it)->getId() == id) {
            m_pending.erase(it);
            return;
        }
    }
    throw ExceptionOutOfRange("Processor::removeFilter: filter ID not found");
}

void Processor::replaceFilterById(uint32_t id, std::unique_ptr<Filter> filter) {
    for (auto &f : m_pending) {
        if (f->getId() == id) {
            f = std::shared_ptr<Filter>(std::move(filter));
            return;
        }
    }
    throw ExceptionOutOfRange("Processor::replaceFilterById: filter ID not found");
}

void Processor::insertAndOverwriteFilter(size_t pos, std::unique_ptr<Filter> filter) {
    if (pos >= m_pending.size())
        throw ExceptionOutOfRange("Processor::insertAndOverwriteFilter: pos out of range");
    m_pending[pos] = std::shared_ptr<Filter>(std::move(filter));
}

Filter *Processor::getFilter(uint32_t id) {
    for (auto &f : m_pending)
        if (f->getId() == id)
            return f.get();
    return nullptr;
}

size_t Processor::size() const {
    return m_pending.size();
}

void Processor::clearAllFilters() {
    m_pending.clear();
}

void Processor::commit() {
    if (!m_running) {
        m_active = m_pending;
        return;
    }
    m_dirty.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> lock(m_mutex);
    m_swapDone.wait(lock, [this] { return !m_dirty.load(std::memory_order_relaxed); });
}

void Processor::setPreFilter(std::unique_ptr<Filter> f)  { m_preFilter  = std::move(f); }
void Processor::setPostFilter(std::unique_ptr<Filter> f) { m_postFilter = std::move(f); }

/* --- processing thread ---------------------------------------------------- */

void Processor::processingThread() {
    while (m_running) {
        Frame *in = nullptr;
        try {
            in = m_inputBuffer.pop();
        } catch (const ExceptionQueueEmpty &) {
            rprint("Processor: input buffer force-notified, exiting\n");
            break;
        }
        if (!in) break;

        Frame *out = nullptr;
        try {
            out = m_outputBuffer.borrow();
        } catch (const ExceptionPoolExhausted &) {
            rprint("Processor: pool exhausted, skipping frame\n");
            m_inputBuffer.giveBack(in);
            continue;
        }

        if (m_dirty.load(std::memory_order_acquire)) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_active = m_pending;
                m_dirty.store(false, std::memory_order_relaxed);
            }
            m_swapDone.notify_one();
        }

        cv::Mat *src = m_preFilter ? &m_preFilter->filter(in->mat(), false) : &in->mat();
        try {
            for (auto &f : m_active)
                src = &f->filter(*src, false);
            if (m_postFilter)
                m_postFilter->filter(*src, &out->mat());
            else
                src->copyTo(out->mat());
        } catch (const std::exception &e) {
            rprint("Processor: filter exception: %s — pipeline cleared\n", e.what());
            m_active.clear();
        }

        try {
            m_outputBuffer.push(out);
        } catch (const ExceptionQueueFull &) {
            rprint("Processor: queue full, dropping frame\n");
            m_outputBuffer.giveBack(out);
        }

        m_inputBuffer.giveBack(in);
    }
}

} // namespace FPGAlix
