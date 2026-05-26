#include "FilterAWB.hpp"
#include "../exception/Exceptions.hpp"

FilterAWB::FilterAWB(float clip_percent)
    : m_clipPercent(clip_percent) {}

cv::Mat& FilterAWB::filter(cv::Mat& input, bool preserveInput) {
    if (input.type() != CV_8UC3)
        throw FPGAlix::ExceptionInvalidFormat("FilterAWB::filter: expected CV_8UC3 input");

    cv::Mat* dstPtr = preserveInput ? &m_dst : &input;

    cv::Mat& dst = *dstPtr;

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

    // Pass 2: apply LUTs, reading from input and writing to dst.
    for (int r = 0; r < input.rows; r++) {
        const uint8_t* src = input.ptr<uint8_t>(r);
        uint8_t*       out = dst.ptr<uint8_t>(r);
        for (int c = 0; c < input.cols * 3; c += 3) {
            out[c    ] = lut[0][src[c    ]];
            out[c + 1] = lut[1][src[c + 1]];
            out[c + 2] = lut[2][src[c + 2]];
        }
    }

    return dst;
}
