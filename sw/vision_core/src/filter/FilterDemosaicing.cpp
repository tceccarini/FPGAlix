#include "FilterDemosaicing.hpp"
#include "../exception/Exceptions.hpp"

void FilterDemosaicing::validateBayerCode(cv::ColorConversionCodes code) {
    switch (code) {
        case cv::COLOR_BayerBG2BGR: case cv::COLOR_BayerGB2BGR:
        case cv::COLOR_BayerRG2BGR: case cv::COLOR_BayerGR2BGR:
        case cv::COLOR_BayerBG2BGR_VNG: case cv::COLOR_BayerGB2BGR_VNG:
        case cv::COLOR_BayerRG2BGR_VNG: case cv::COLOR_BayerGR2BGR_VNG:
        case cv::COLOR_BayerBG2BGR_EA:  case cv::COLOR_BayerGB2BGR_EA:
        case cv::COLOR_BayerRG2BGR_EA:  case cv::COLOR_BayerGR2BGR_EA:
        case cv::COLOR_BayerBG2GRAY: case cv::COLOR_BayerGB2GRAY:
        case cv::COLOR_BayerRG2GRAY: case cv::COLOR_BayerGR2GRAY:
            return;
        default:
            throw FPGAlix::ExceptionInvalidFormat("FilterDemosaicing: code must be a Bayer-to-BGR or Bayer-to-GRAY conversion code");
    }
}

FilterDemosaicing::FilterDemosaicing(cv::ColorConversionCodes code)
    : m_code(code) {
    validateBayerCode(code);
}

cv::Mat& FilterDemosaicing::filter(cv::Mat& input, bool /* preserveInput */) {
    // Strip processing with 2-row overlap so each strip has correct neighbor pixels
    // at its boundaries (bilinear needs 1, VNG needs 2). kStrip=32 (even) keeps
    // Bayer phase aligned: y is always even, so y_ext = y-2 is also always even.
    constexpr int kStrip  = 32;
    constexpr int kBorder = 2;

    bool toGray = (m_code == cv::COLOR_BayerBG2GRAY || m_code == cv::COLOR_BayerGB2GRAY ||
                   m_code == cv::COLOR_BayerRG2GRAY || m_code == cv::COLOR_BayerGR2GRAY);
    m_dst.create(input.size(), toGray ? CV_8UC1 : CV_8UC3);

    for (int y = 0; y < input.rows; y += kStrip) {
        int h      = std::min(kStrip, input.rows - y);
        int y_ext  = std::max(0, y - kBorder);
        int h_ext  = std::min(input.rows, y + h + kBorder) - y_ext;
        cv::Mat strip_out;
        cv::cvtColor(input.rowRange(y_ext, y_ext + h_ext), strip_out, m_code);
        strip_out.rowRange(y - y_ext, y - y_ext + h).copyTo(m_dst.rowRange(y, y + h));
    }
    return m_dst;
}
