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
    cv::cvtColor(input, m_dst, m_code);
    return m_dst;
}
