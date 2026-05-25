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

cv::Mat& FilterDemosaicing::filter(cv::Mat& input) {
    return filter(input, nullptr);
}

cv::Mat& FilterDemosaicing::filter(cv::Mat& input, cv::Mat* output, bool /* preserveInput */) {
    cv::Mat& dst = output ? *output : m_dst;
    cv::cvtColor(input, dst, m_code);
    return dst;
}
