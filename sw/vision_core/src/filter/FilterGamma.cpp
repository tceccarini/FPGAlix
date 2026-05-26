#include "FilterGamma.hpp"
#include "../exception/Exceptions.hpp"
#include <cmath>

FilterGamma::FilterGamma(float gamma, float gain) {
    // 1/gamma = encoding direction (e.g. gamma=2.2 → exponent≈0.45, brightens midtones).
    // Precomputed here so filter() has zero floating-point cost at runtime.
    for (int i = 0; i < 256; i++)
        m_lut[i] = cv::saturate_cast<uint8_t>(std::pow(i / 255.0f, 1.0f / gamma) * 255.0f * gain);
}

cv::Mat& FilterGamma::filter(cv::Mat& input, bool preserveInput) {
    if (input.depth() != CV_8U)
        throw FPGAlix::ExceptionInvalidFormat("FilterGamma::filter: expected CV_8U depth");

    cv::Mat& dst = preserveInput ? m_dst : input;

    if (input.isContinuous() && dst.isContinuous()) {
        const uint8_t* src = input.data;
        uint8_t*       out = dst.data;
        const int n = input.rows * input.cols * input.channels();
        for (int i = 0; i < n; i++)
            out[i] = m_lut[src[i]];
    } else {
        const int stride = input.cols * input.channels();
        for (int r = 0; r < input.rows; r++) {
            const uint8_t* src = input.ptr<uint8_t>(r);
            uint8_t*       out = dst.ptr<uint8_t>(r);
            for (int c = 0; c < stride; c++)
                out[c] = m_lut[src[c]];
        }
    }

    return dst;
}
