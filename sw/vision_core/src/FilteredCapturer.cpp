#include "FilteredCapturer.hpp"

namespace FPGAlix {

FilteredCapturer::FilteredCapturer(FrameBuffer &buffer)
    : Capturer(buffer) {}

/* --- pipeline editing ----------------------------------------------------- */

void FilteredCapturer::appendFilter(std::unique_ptr<Filter> filter) {
    m_pending.push_back(std::shared_ptr<Filter>(std::move(filter)));
}

void FilteredCapturer::prependFilter(std::unique_ptr<Filter> filter) {
    insertFilter(0, std::move(filter));
}

void FilteredCapturer::insertFilter(size_t pos, std::unique_ptr<Filter> filter) {
    if (pos > m_pending.size())
        throw ExceptionOutOfRange("FilteredCapturer::insertFilter: pos out of range");
    m_pending.insert(m_pending.begin() + static_cast<std::ptrdiff_t>(pos),
                     std::shared_ptr<Filter>(std::move(filter)));
}

void FilteredCapturer::removeFilter(uint32_t id) {
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if ((*it)->getId() == id) {
            m_pending.erase(it);
            return;
        }
    }
    throw ExceptionOutOfRange("FilteredCapturer::removeFilter: filter ID not found");
}

void FilteredCapturer::replaceFilterById(uint32_t id, std::unique_ptr<Filter> filter) {
    for (auto &f : m_pending) {
        if (f->getId() == id) {
            f = std::shared_ptr<Filter>(std::move(filter));
            return;
        }
    }
    throw ExceptionOutOfRange("FilteredCapturer::replaceFilterById: filter ID not found");
}

void FilteredCapturer::insertAndOverwriteFilter(size_t pos, std::unique_ptr<Filter> filter) {
    if (pos >= m_pending.size())
        throw ExceptionOutOfRange("FilteredCapturer::insertAndOverwriteFilter: pos out of range");
    m_pending[pos] = std::shared_ptr<Filter>(std::move(filter));
}

Filter *FilteredCapturer::getFilter(uint32_t id) {
    for (auto &f : m_pending)
        if (f->getId() == id)
            return f.get();
    return nullptr;
}

size_t FilteredCapturer::size() const {
    return m_pending.size();
}

void FilteredCapturer::commit() {
    if (!isRunning()) {
        m_active = m_pending;
        return;
    }
    m_dirty.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> lock(m_mutex);
    m_swapDone.wait(lock, [this] { return !m_dirty.load(std::memory_order_relaxed); });
}

/* --- capture thread ------------------------------------------------------- */

void FilteredCapturer::process(cv::Mat &mat_in, cv::Mat &mat_out) {
    if (m_dirty.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_active = m_pending;
            m_dirty.store(false, std::memory_order_relaxed);
        }
        m_swapDone.notify_one();
    }

    if (m_active.empty()) {
        mat_in.copyTo(mat_out);
        return;
    }

    cv::Mat *src = &mat_in;
    for (size_t i = 0; i + 1 < m_active.size(); ++i)
        src = &m_active[i]->filter(*src);
    m_active.back()->filter(*src, &mat_out, /*preserveInput=*/true);
}

} // namespace FPGAlix
