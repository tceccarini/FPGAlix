#pragma once

#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Converts a raw Bayer-pattern frame (CV_8UC1) to a full-color cv::Mat
// using OpenCV's cv::cvtColor. Accepted output formats are BGR and GRAY.
// Throws FPGAlix::ExceptionInvalidFormat at construction if the code is
// not a recognized Bayer demosaicing code.
class FilterDemosaicing : public Filter {
public:
    // code: encodes both the Bayer tile layout and the demosaicing algorithm.
    //   Layout:    BayerBG, BayerGB, BayerRG, BayerGR
    //   Algorithm: (default bilinear), _VNG, _EA
    //   Output:    2BGR or 2GRAY
    // Example: cv::COLOR_BayerBG2BGR_VNG
    explicit FilterDemosaicing(cv::ColorConversionCodes code);

    // Expects a CV_8UC1 Bayer-mosaiced input.
    // Returns a reference to an internal mat with the demosaiced result.
    cv::Mat& filter(cv::Mat& input) override;
    // Writes the demosaiced result into *output if provided, otherwise into the internal mat.
    cv::Mat& filter(cv::Mat& input, cv::Mat* output, bool preserveInput = false) override;

private:
    // Throws if code is not a Bayer-to-BGR or Bayer-to-GRAY conversion.
    // Called at construction so invalid codes are caught early.
    static void validateBayerCode(cv::ColorConversionCodes code);

    cv::Mat m_dst;
    cv::ColorConversionCodes m_code;
};
