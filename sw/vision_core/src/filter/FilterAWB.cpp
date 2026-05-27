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

    // Pass 1: accumulate per-channel histograms over 32-row strips to keep
    // DDR3 bursts small and reduce contention with mSGDMA write traffic.
    constexpr int kStrip   = 32;
    const int   histSize   = 256;
    const float range[]    = {0.0f, 256.0f};
    const float *ranges[]  = {range};
    cv::Mat hists[3];
    for (int y = 0; y < input.rows; y += kStrip) {
        int     h     = std::min(kStrip, input.rows - y);
        cv::Mat strip = input.rowRange(y, y + h);
        for (int ch = 0; ch < 3; ch++) {
            cv::Mat tmp;
            cv::calcHist(&strip, 1, &ch, cv::Mat(), tmp, 1, &histSize, ranges);
            hists[ch].empty() ? hists[ch] = tmp : hists[ch] += tmp;
        }
    }

    // Build one LUT per channel on the stack.
    uint8_t lut[3][256];
    for (int ch = 0; ch < 3; ch++) {
        const float *h = reinterpret_cast<const float*>(hists[ch].data);
        int low = 0, cum = 0;
        while (low < 255 && (cum += static_cast<int>(h[low])) < clip_low) low++;

        int high = 255; cum = total;
        while (high > 0 && (cum -= static_cast<int>(h[high])) >= clip_hi) high--;

        float scale = (high > low) ? 255.0f / (high - low) : 0.0f;
        for (int i = 0; i < 256; i++)
            lut[ch][i] = cv::saturate_cast<uint8_t>((i - low) * scale);
    }

    // Pass 2: apply LUT per strip (TBB + NEON per strip).
    cv::Mat lut_mat(1, 256, CV_8UC3);
    uint8_t *p = lut_mat.data;
    for (int i = 0; i < 256; i++) {
        p[i * 3    ] = lut[0][i];
        p[i * 3 + 1] = lut[1][i];
        p[i * 3 + 2] = lut[2][i];
    }
    m_dst.create(input.size(), input.type());
    for (int y = 0; y < input.rows; y += kStrip) {
        int h = std::min(kStrip, input.rows - y);
        cv::LUT(input.rowRange(y, y + h), lut_mat, m_dst.rowRange(y, y + h));
    }
    return m_dst;
}
