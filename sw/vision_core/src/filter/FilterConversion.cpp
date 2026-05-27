#include "FilterConversion.hpp"
#include "exception/ExceptionInvalidFormat.hpp"
#include <opencv2/imgproc.hpp>

FilterConversion::FilterConversion(int outputType) : m_outputType(outputType) {}

void FilterConversion::filter(cv::Mat& input, cv::Mat* output) {
    constexpr int kStrip = 32;
    if (input.type() == CV_8UC3 && m_outputType == CV_8UC1) {
        for (int y = 0; y < input.rows; y += kStrip) {
            int h = std::min(kStrip, input.rows - y);
            cv::cvtColor(input.rowRange(y, y + h), output->rowRange(y, y + h), cv::COLOR_BGR2GRAY);
        }
    } else if (input.type() == CV_8UC1 && m_outputType == CV_8UC3) {
        for (int y = 0; y < input.rows; y += kStrip) {
            int h = std::min(kStrip, input.rows - y);
            cv::cvtColor(input.rowRange(y, y + h), output->rowRange(y, y + h), cv::COLOR_GRAY2BGR);
        }
    } else if (input.type() == m_outputType) {
        input.copyTo(*output);
    } else {
        throw FPGAlix::ExceptionInvalidFormat("FilterConversion: unsupported type combination");
    }
}

cv::Mat& FilterConversion::filter(cv::Mat& input, bool preserveInput) {
    if (input.type() == m_outputType && !preserveInput)
        return input;
    m_dst.create(input.rows, input.cols, m_outputType);
    filter(input, &m_dst);
    return m_dst;
}
