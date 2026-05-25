#include "FilterGamma.hpp"
#include "../exception/Exceptions.hpp"
#include <cmath>

FilterGamma::FilterGamma(float gamma, float gain) {
    // 1/gamma = encoding direction (e.g. gamma=2.2 → exponent≈0.45, brightens midtones).
    // Precomputed here so filter() has zero floating-point cost at runtime.
    for (int i = 0; i < 256; i++)
        m_lut[i] = cv::saturate_cast<uint8_t>(std::pow(i / 255.0f, 1.0f / gamma) * 255.0f * gain);
}

cv::Mat& FilterGamma::filter(cv::Mat& input) {
    return filter(input, nullptr);
}

cv::Mat& FilterGamma::filter(cv::Mat& input, cv::Mat* output, bool preserveInput) {
    if (input.type() != CV_8UC3)
        throw FPGAlix::ExceptionInvalidFormat("FilterGamma::filter: expected CV_8UC3 input");

    cv::Mat* dstPtr;
    if (!preserveInput)
        dstPtr = &input;
    else if (output != nullptr)
        dstPtr = output;
    else
        dstPtr = &m_dst;

    cv::Mat& dst = *dstPtr;

    // Flat loop on continuous data avoids per-row ptr() overhead and
    // gives the Cortex-A9 prefetcher a single uninterrupted stride.
    if (input.isContinuous() && dst.isContinuous()) {
        const uint8_t* src = input.data;
        uint8_t*       out = dst.data;
        int n = input.rows * input.cols * 3;
        for (int i = 0; i < n; i++)
            out[i] = m_lut[src[i]];
    } else {
        for (int r = 0; r < input.rows; r++) {
            const uint8_t* src = input.ptr<uint8_t>(r);
            uint8_t*       out = dst.ptr<uint8_t>(r);
            for (int c = 0; c < input.cols * 3; c++)
                out[c] = m_lut[src[c]];
        }
    }

    return dst;
}
