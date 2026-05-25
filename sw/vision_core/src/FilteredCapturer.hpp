#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include "Capturer.hpp"
#include "filter/Filter.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

/* Extends Capturer with an owned, double-buffered filter pipeline.
   The pipeline runs top-to-bottom: position 0 is applied first,
   position size()-1 is applied last.

   Caller thread: modifies m_pending via append/insert/remove/replace,
   then calls commit() to schedule the new pipeline.

   Capture thread: at the start of the first frame after commit(), swaps
   m_pending into m_active and runs the filter chain — with no lock held
   during actual filtering.

   openDevice() remains pure-virtual: a concrete subclass must implement it. */
class FilteredCapturer : public Capturer {
public:
    explicit FilteredCapturer(FrameBuffer &buffer);
    ~FilteredCapturer() override = default;

    /* --- pipeline editing (caller thread) --------------------------------- */

    /* Takes ownership of the filter and adds it at the bottom of the pipeline. */
    void appendFilter(std::unique_ptr<Filter> filter);

    /* Takes ownership of the filter and adds it at the top of the pipeline (position 0). */
    void prependFilter(std::unique_ptr<Filter> filter);

    /* Takes ownership of the filter and inserts it at position pos.
       Existing filters at pos and below are shifted down by one.
       Throws ExceptionOutOfRange if pos > size(). */
    void insertFilter(size_t pos, std::unique_ptr<Filter> filter);

    /* Removes the filter with the given ID from the pipeline and destroys it.
       Throws ExceptionOutOfRange if no filter with that ID exists. */
    void removeFilter(uint32_t id);

    /* Replaces the filter with the given ID with a new one (takes ownership).
       The old filter is destroyed. Throws ExceptionOutOfRange if ID not found. */
    void replaceFilterById(uint32_t id, std::unique_ptr<Filter> filter);

    /* Replaces the filter at position pos with a new one (takes ownership).
       The old filter is destroyed. Throws ExceptionOutOfRange if pos >= size(). */
    void insertAndOverwriteFilter(size_t pos, std::unique_ptr<Filter> filter);

    /* Returns a non-owning pointer to the filter with the given ID in the
       pending pipeline, or nullptr if no filter with that ID exists. */
    Filter *getFilter(uint32_t id);

    /* Returns the number of filters currently in the pending pipeline.
       Valid positions for insertFilter are [0, size()]. */
    size_t size() const;

    /* Blocks until process() has swapped the pending pipeline into the active
       one. After commit() returns, m_pending is safe to modify again. */
    void commit();

    /* --- capture thread --------------------------------------------------- */

    /* Swaps in the pending pipeline if commit() was called, then applies
       the active filter chain. No lock is held during filtering. */
    void process(cv::Mat &mat_in, cv::Mat &mat_out) override;

private:
    using FilterList = std::vector<std::shared_ptr<Filter>>;

    FilterList m_active;   // used exclusively by process()
    FilterList m_pending;  // modified by the caller; protected by m_mutex

    std::atomic<bool>        m_dirty{false};
    std::mutex               m_mutex;     // guards m_pending and the swap
    std::condition_variable  m_swapDone;  // notified by process() after swap
};

} // namespace FPGAlix
