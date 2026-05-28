#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "FrameBuffer.hpp"
#include "filter/Filter.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

/* Standalone processing stage: pops frames from inputBuffer, runs them through
   a double-buffered filter pipeline, and pushes results into outputBuffer.

   Pipeline editing and commit() semantics mirror FilteredCapturer:
   modify the pending pipeline via append/insert/remove/replace, then call
   commit() to schedule the swap. The swap happens at the start of the next
   frame in the processing thread, with no lock held during filtering. */
class Processor {
public:
    Processor(FrameBuffer &inputBuffer, FrameBuffer &outputBuffer);
    ~Processor();

    /* Spawns the processing thread. Must be called after pipeline setup. */
    void start();

    /* Signals the thread to stop, unblocks pop() and joins. */
    void stop();

    /* --- pipeline editing (caller thread) --------------------------------- */

    /* Takes ownership of the filter and appends it at the bottom of the pipeline. */
    void appendFilter(std::unique_ptr<Filter> filter);

    /* Takes ownership of the filter and inserts it at the top (position 0). */
    void prependFilter(std::unique_ptr<Filter> filter);

    /* Takes ownership of the filter and inserts it at position pos.
       Existing filters at pos and below are shifted down by one.
       Throws ExceptionOutOfRange if pos > size(). */
    void insertFilter(size_t pos, std::unique_ptr<Filter> filter);

    /* Removes the filter with the given ID and destroys it.
       Throws ExceptionOutOfRange if no filter with that ID exists. */
    void removeFilter(uint32_t id);

    /* Replaces the filter with the given ID with a new one (takes ownership).
       Throws ExceptionOutOfRange if ID not found. */
    void replaceFilterById(uint32_t id, std::unique_ptr<Filter> filter);

    /* Replaces the filter at position pos with a new one (takes ownership).
       Throws ExceptionOutOfRange if pos >= size(). */
    void insertAndOverwriteFilter(size_t pos, std::unique_ptr<Filter> filter);

    /* Returns a non-owning pointer to the filter with the given ID in the
       pending pipeline, or nullptr if not found. */
    Filter *getFilter(uint32_t id);

    /* Returns the number of filters in the pending pipeline. */
    size_t size() const;

    /* Removes all filters from the pending pipeline.
       Must be followed by commit() to take effect. */
    void clearAllFilters();

    /* Blocks until the processing thread has swapped the pending pipeline
       into the active one. Safe to modify the pipeline again after returning. */
    void commit();

    /* Fixed filters applied outside the user pipeline. Applied every frame
       regardless of commit(); ownership is transferred to Processor. */
    void setPreFilter(std::unique_ptr<Filter> filter);
    void setPostFilter(std::unique_ptr<Filter> filter);

private:
    void processingThread();

    FrameBuffer      &m_inputBuffer;
    FrameBuffer      &m_outputBuffer;

    using FilterList = std::vector<std::shared_ptr<Filter>>;
    FilterList        m_active;   /* used exclusively by processingThread() */
    FilterList        m_pending;  /* modified by the caller; protected by m_mutex */
    std::unique_ptr<Filter> m_preFilter;
    std::unique_ptr<Filter> m_postFilter;

    std::atomic<bool>        m_dirty{false};
    std::mutex               m_mutex;    /* guards m_pending and the swap */
    std::condition_variable  m_swapDone; /* notified by process() after swap */

    std::atomic<bool>        m_running{false};
    std::thread              m_thread;
};

} // namespace FPGAlix
