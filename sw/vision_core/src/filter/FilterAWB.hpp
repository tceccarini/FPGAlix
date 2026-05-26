#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Auto White Balance using GIMP-style per-channel percentile stretch.
// For each BGR channel independently:
//   1. Computes the pixel value histogram.
//   2. Clips clip_percent of pixels at each end of the histogram.
//   3. Linearly stretches the remaining range to [0, 255].
//
// preserveInput=false (default): modifies input in place and returns it.
// preserveInput=true:            writes result to an internal buffer, input untouched.
// Throws FPGAlix::ExceptionInvalidFormat if input is not CV_8UC3.
//
// Implementation uses two passes over the raw pixel data with stack-allocated
// LUTs to avoid any heap allocation during filter().
class FilterAWB : public Filter {
public:
    // clip_percent: fraction of pixels clipped at each histogram end.
    // 0.006 matches GIMP's default "Colors > Auto > White Balance".
    explicit FilterAWB(float clip_percent = 0.006f);

    cv::Mat& filter(cv::Mat& input, bool preserveInput) override;

private:
    float   m_clipPercent;
    cv::Mat m_dst;   // used when preserveInput=true
};
