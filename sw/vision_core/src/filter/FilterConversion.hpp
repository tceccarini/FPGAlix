#pragma once

#include "Filter.hpp"
#include <opencv2/core.hpp>

// Converts a frame to a fixed output type (CV_8UC1 / CV_8UC3).
// The conversion code is chosen automatically from the input type at filter time:
//   CV_8UC3 → CV_8UC1 : BGR2GRAY
//   CV_8UC1 → CV_8UC3 : GRAY2BGR
//   same type          : copy (or no-op mid-pipeline)
class FilterConversion : public Filter {
public:
    explicit FilterConversion(int outputType);

    cv::Mat& filter(cv::Mat& input, bool preserveInput) override;

    // Writes the result directly into *output — intended for the last filter
    // in a pipeline writing into a pre-allocated buffer (e.g. FrameBuffer frame).
    // Type compatibility is the caller's responsibility.
    void filter(cv::Mat& input, cv::Mat* output);

private:
    int     m_outputType;
    cv::Mat m_dst;
};
