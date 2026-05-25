#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Applies gamma correction and a linear gain to a CV_8UC3 BGR frame in place.
// The full input→output mapping is collapsed into a 256-entry LUT at construction,
// so filter() performs only integer table lookups with zero floating-point cost.
// Throws FPGAlix::ExceptionInvalidFormat if input is not CV_8UC3.
class FilterGamma : public Filter {
public:
    // gamma: power-law exponent (e.g. 2.2 for standard sRGB encoding).
    //        Values > 1 brighten midtones; values < 1 darken them.
    // gain:  linear multiplier applied after gamma (1.0 = no change).
    //        Saturates to 255 — use values > 1.0 to boost brightness.
    FilterGamma(float gamma, float gain);

    // Modifies input in place and returns it.
    cv::Mat& filter(cv::Mat& input) override;
    // In-place — output is ignored. Input is modified and returned.
    cv::Mat& filter(cv::Mat& input, cv::Mat* output, bool preserveInput = false) override;

private:
    uint8_t  m_lut[256]; // precomputed gamma+gain mapping for all 256 input values
    cv::Mat  m_dst;      // used when preserveInput=true and output==nullptr
};
