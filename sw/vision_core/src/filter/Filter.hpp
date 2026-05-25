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

    // Apply the filter to the given frame. May modify the input mat in place;
    // this is encouraged to avoid unnecessary memory copies.
    // Returns a reference to the result — either input itself or an internal mat.
    virtual cv::Mat& filter(cv::Mat& input) = 0;

    // Same as filter(input) but lets the caller decide where the result lands:
    //   output == nullptr → identical to filter(input): in-place or internal m_dst.
    //   output != nullptr → result is written into *output and *output is returned.
    //                       If the filter normally works in-place, input is
    //                       modified and returned — output is ignored.
    // Pass a non-null output to write directly into caller-owned memory
    // (e.g. a cv::Mat wrapping a GStreamer buffer) and avoid an extra copy.
    // preserveInput: when true, input is never modified. The result is written
    // into *output if output != nullptr, otherwise into an internal buffer.
    virtual cv::Mat& filter(cv::Mat& input, cv::Mat* output, bool preserveInput = false) = 0;

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
