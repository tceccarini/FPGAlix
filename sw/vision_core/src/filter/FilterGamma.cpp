#include "FilterGamma.hpp"
#include "../exception/Exceptions.hpp"
#include <cmath>

FilterGamma::FilterGamma(float gamma, float gain) {
    m_lut.create(1, 256, CV_8UC1);
    uint8_t *p = m_lut.data;
    for (int i = 0; i < 256; i++)
        p[i] = cv::saturate_cast<uint8_t>(std::pow(i / 255.0f, 1.0f / gamma) * 255.0f * gain);
}

cv::Mat& FilterGamma::filter(cv::Mat& input, bool preserveInput) {
    if (input.depth() != CV_8U)
        throw FPGAlix::ExceptionInvalidFormat("FilterGamma::filter: expected CV_8U depth");
    cv::LUT(input, m_lut, m_dst);
    return m_dst;
}
