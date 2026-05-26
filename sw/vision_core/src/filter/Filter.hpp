#pragma once

#include <atomic>
#include <cstdint>
#include <opencv2/core.hpp>

// Base class for image filters applied to a cv::Mat frame.
// Subclasses implement filter() to transform a single frame.
// The contract encourages in-place modification of input to avoid copies:
// returning input directly is valid and preferred when possible.
class Filter {
public:
    virtual ~Filter() = default;

    // Returns the unique ID assigned to this instance at construction.
    // IDs are monotonically increasing and unique across all Filter subclasses
    // within the same process lifetime.
    uint32_t getId() const;

    // Apply the filter to the given frame.
    // preserveInput = false (default): the filter may modify input in place.
    // preserveInput = true:            input is never touched; result goes to an internal buffer.
    // Returns a reference to the result — either input itself or an internal mat.
    virtual cv::Mat& filter(cv::Mat& input, bool preserveInput) = 0;

protected:
    // Each construction atomically claims the next available ID from the
    // shared counter, ensuring uniqueness even when filters are created
    // concurrently from multiple threads.
    Filter() : m_filterId(s_nextId++) {}

private:
    // Global counter shared by all Filter instances; incremented atomically.
    inline static std::atomic<uint32_t> s_nextId{0};
    // Immutable after construction — set once in the initialiser list.
    uint32_t m_filterId;
};
