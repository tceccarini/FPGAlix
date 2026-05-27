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
    constexpr int kStrip = 32;
    m_dst.create(input.size(), input.type());
    for (int y = 0; y < input.rows; y += kStrip) {
        int h = std::min(kStrip, input.rows - y);
        cv::LUT(input.rowRange(y, y + h), m_lut, m_dst.rowRange(y, y + h));
    }
    return m_dst;
}
