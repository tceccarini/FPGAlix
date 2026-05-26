#pragma once
#include <mutex>
#include <condition_variable>
#include "Frame.hpp"
#include "exception/Exceptions.hpp"

namespace FPGAlix {

class FrameBuffer {
public:
    /* Allocates 'size' Frame objects (pool) and a queue of 'size' pointers.
       All memory is owned here — no external allocation needed. */
    FrameBuffer(int width, int height, int type, int size);
    virtual ~FrameBuffer();

    /* Producer: scans the pool for a free frame, marks it busy and returns it.
       Throws ExceptionPoolExhausted if all frames are in use. */
    Frame *borrow();

    /* Producer: enqueues a filled frame pointer for the Streamer to consume.
       Throws ExceptionQueueFull if the queue is full.
       Wakes up any thread blocked in pop(). */
    void push(Frame *frame);

    /* Streamer: blocks until a frame is available, then returns it.
       Use pop() in a dedicated thread — it may wait indefinitely. */
    Frame *pop();

    /* GStreamer destroy_notify: marks the frame as free so borrow() can reuse it.
       NOTE: with the current implementation this is equivalent to calling
       frame->clearBusy() directly — giveBack() exists to allow future
       implementations to add extra logic on release without changing callers. */
    void giveBack(Frame *frame);

    /* Unblocks any thread waiting in pop() — call from Streamer::stop().
       Sets m_forceNotify to true and wakes the condition variable. */
    void forceNotify();

    /* Returns the frame at the given pool index. Valid after construction. */
    const Frame& getFrame(int index) const;

    /* Returns the pool capacity — total number of pre-allocated Frame slots. */
    int getSize() const;

private:
    Frame **m_pool;    /* pre-allocated frame objects */
    Frame **m_queue;   /* circular FIFO of Frame pointers into m_pool */
    int     m_size;    /* pool and queue capacity */

    int m_poolHead{0};   /* next slot to scan in borrow() */
    int m_queueHead{0};  /* next write position in push() */
    int m_queueTail{0};  /* next read  position in pop()  */
    int m_queueSize{0};  /* number of frames currently in the queue */

    std::mutex              m_mutex;              /* protects all index, size and flag fields */
    std::condition_variable m_cv;                 /* signals pop() when push() adds a frame */
    bool                    m_forceNotify{false};  /* set by notify() to unblock pop() — no atomic needed: always accessed under m_mutex */

    /* Private helpers — always called with m_mutex already held */
    bool isQueueEmpty() const { return m_queueSize == 0; }
    bool isQueueFull()  const { return m_queueSize == m_size; }
};

} // namespace FPGAlix
