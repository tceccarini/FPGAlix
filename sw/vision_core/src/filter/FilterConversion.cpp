#include "FilterConversion.hpp"

FilterConversion::FilterConversion(cv::ColorConversionCodes code)
    : m_code(code) {}

cv::Mat& FilterConversion::filter(cv::Mat& input) {
    return filter(input, nullptr);
}

cv::Mat& FilterConversion::filter(cv::Mat& input, cv::Mat* output, bool /* preserveInput */) {
    cv::Mat& dst = output ? *output : m_dst;

    // If input and output types match (learned after the first call), the
    // conversion is a no-op from a type perspective — return input directly.
    if (m_outputType != -1 && input.type() == m_outputType)
        return input;

    cv::cvtColor(input, dst, m_code);
    m_outputType = dst.type();
    return dst;
}
