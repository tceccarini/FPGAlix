#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Applies gamma correction and a linear gain per pixel using a 256-entry LUT.
// Accepts any CV_8U depth (CV_8UC1, CV_8UC3, CV_8UC4); operates on raw bytes
// regardless of channel count. Zero floating-point cost at runtime.
// Throws FPGAlix::ExceptionInvalidFormat if input depth is not CV_8U.
class FilterGamma : public Filter {
public:
    // gamma: power-law exponent (e.g. 2.2 for standard sRGB encoding).
    //        Values > 1 brighten midtones; values < 1 darken them.
    // gain:  linear multiplier applied after gamma (1.0 = no change).
    FilterGamma(float gamma, float gain);

    cv::Mat& filter(cv::Mat& input, bool preserveInput) override;

private:
    cv::Mat  m_lut; // precomputed gamma+gain mapping, CV_8UC1 1×256
    cv::Mat  m_dst;
};
