#include "FilterKeepMosaic.hpp"
#include "../exception/Exceptions.hpp"
#include <array>

// Maps a Bayer code to the BGR channel index for each of the 4 tile positions:
// index 0 = (row even, col even), 1 = (row even, col odd),
// index 2 = (row odd,  col even), 3 = (row odd,  col odd).
std::array<int, 4> FilterKeepMosaic::bayerChannels(cv::ColorConversionCodes code) {
    if (code == cv::COLOR_BayerBG2BGR || code == cv::COLOR_BayerBG2BGR_VNG || code == cv::COLOR_BayerBG2BGR_EA)
        return {0, 1, 1, 2}; // B G / G R
    if (code == cv::COLOR_BayerGB2BGR || code == cv::COLOR_BayerGB2BGR_VNG || code == cv::COLOR_BayerGB2BGR_EA)
        return {1, 0, 2, 1}; // G B / R G
    if (code == cv::COLOR_BayerRG2BGR || code == cv::COLOR_BayerRG2BGR_VNG || code == cv::COLOR_BayerRG2BGR_EA)
        return {2, 1, 1, 0}; // R G / G B
    if (code == cv::COLOR_BayerGR2BGR || code == cv::COLOR_BayerGR2BGR_VNG || code == cv::COLOR_BayerGR2BGR_EA)
        return {1, 2, 0, 1}; // G R / B G
    throw FPGAlix::ExceptionInvalidFormat("FilterKeepMosaic: unrecognized Bayer pattern code");
}

FilterKeepMosaic::FilterKeepMosaic(int output_format, cv::ColorConversionCodes bayer_code)
    : m_outputFormat(output_format), m_bayerCode(bayer_code), m_bayerChannels(bayerChannels(bayer_code)) {
    if (output_format != CV_8UC1 && output_format != CV_8UC3)
        throw FPGAlix::ExceptionInvalidFormat("FilterKeepMosaic: unsupported format, expected CV_8UC1 or CV_8UC3");
}

cv::Mat& FilterKeepMosaic::filter(cv::Mat& input, bool /* preserveInput */) {
    if (input.type() != CV_8UC1)
        throw FPGAlix::ExceptionInvalidFormat("FilterKeepMosaic::filter: expected CV_8UC1 input (raw Bayer mosaic)");

    switch (m_outputFormat) {
        case CV_8UC1:
            return input;
        case CV_8UC3: {
            if (m_dst.empty()) {
                m_dst.create(input.size(), CV_8UC3);
                m_dst.setTo(cv::Scalar(0, 0, 0));
            }
            for (int r = 0; r < input.rows; r++) {
                const uint8_t* src     = input.ptr<uint8_t>(r);
                cv::Vec3b*     d       = m_dst.ptr<cv::Vec3b>(r);
                // Hoist even/odd channel indices out of the inner loop:
                // the Bayer pattern repeats every 2 columns, so only 2 values alternate.
                int            roff    = (r & 1) * 2;
                int            ch_even = m_bayerChannels[roff];
                int            ch_odd  = m_bayerChannels[roff + 1];
                for (int c = 0; c < input.cols; c += 2) {
                    d[c    ][ch_even] = src[c    ];
                    d[c + 1][ch_odd ] = src[c + 1];
                }
            }
            return m_dst;
        }
        default:
            throw FPGAlix::ExceptionInvalidFormat("FilterKeepMosaic::filter: unsupported format");
    }
}
