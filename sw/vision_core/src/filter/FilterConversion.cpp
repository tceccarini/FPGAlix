#include "FilterConversion.hpp"
#include "exception/ExceptionInvalidFormat.hpp"
#include <opencv2/imgproc.hpp>

FilterConversion::FilterConversion(int outputType) : m_outputType(outputType) {}

void FilterConversion::filter(cv::Mat& input, cv::Mat* output) {
    if (input.type() == m_outputType) {
        input.copyTo(*output);
        return;
    }
    switch (input.type()) {
    case CV_8UC1:
        if (m_outputType == CV_8UC3) {
            cv::cvtColor(input, *output, cv::COLOR_GRAY2BGR);
            return;
        }
        break;
    case CV_8UC2:  // YUYV
        if (m_outputType == CV_8UC3) {
            cv::cvtColor(input, *output, cv::COLOR_YUV2BGR_YUYV);
            return;
        }
        break;
    case CV_8UC3:
        if (m_outputType == CV_8UC1) {
            cv::cvtColor(input, *output, cv::COLOR_BGR2GRAY);
            return;
        }
        break;
    }
    throw FPGAlix::ExceptionInvalidFormat("FilterConversion: unsupported type combination");
}

cv::Mat& FilterConversion::filter(cv::Mat& input, bool preserveInput) {
    if (input.type() == m_outputType && !preserveInput)
        return input;
    m_dst.create(input.rows, input.cols, m_outputType);
    filter(input, &m_dst);
    return m_dst;
}
