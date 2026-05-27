#include "FilterAWB.hpp"
#include "../exception/Exceptions.hpp"

FilterAWB::FilterAWB(float clip_percent)
    : m_clipPercent(clip_percent) {}

cv::Mat& FilterAWB::filter(cv::Mat& input, bool preserveInput) {
    if (input.type() != CV_8UC3)
        throw FPGAlix::ExceptionInvalidFormat("FilterAWB::filter: expected CV_8UC3 input");

    int total    = input.rows * input.cols;
    int clip_low = static_cast<int>(total * m_clipPercent);
    int clip_hi  = total - clip_low;

    // Pass 1: compute histograms from input.
    int hist[3][256] = {};
    for (int r = 0; r < input.rows; r++) {
        const uint8_t* row = input.ptr<uint8_t>(r);
        for (int c = 0; c < input.cols * 3; c += 3) {
            hist[0][row[c    ]]++;
            hist[1][row[c + 1]]++;
            hist[2][row[c + 2]]++;
        }
    }

    // Build one LUT per channel on the stack — no heap allocation.
    uint8_t lut[3][256];
    for (int ch = 0; ch < 3; ch++) {
        int low = 0, cum = 0;
        while (low < 255 && (cum += hist[ch][low]) < clip_low) low++;

        int high = 255; cum = total;
        while (high > 0 && (cum -= hist[ch][high]) >= clip_hi) high--;

        float scale = (high > low) ? 255.0f / (high - low) : 0.0f;
        for (int i = 0; i < 256; i++)
            lut[ch][i] = cv::saturate_cast<uint8_t>((i - low) * scale);
    }

    cv::Mat lut_mat(1, 256, CV_8UC3);
    uint8_t *p = lut_mat.data;
    for (int i = 0; i < 256; i++) {
        p[i * 3    ] = lut[0][i];
        p[i * 3 + 1] = lut[1][i];
        p[i * 3 + 2] = lut[2][i];
    }
    cv::LUT(input, lut_mat, m_dst);
    return m_dst;
}
