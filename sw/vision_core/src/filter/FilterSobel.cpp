#include "FilterSobel.hpp"
#include "exception/ExceptionInvalidFormat.hpp"

FilterSobel::FilterSobel(bool overlay) : m_overlay(overlay) {}

cv::Mat& FilterSobel::filter(cv::Mat& input, bool /* preserveInput */) {
    if (input.type() != CV_8UC1 && input.type() != CV_8UC3)
        throw FPGAlix::ExceptionInvalidFormat("FilterSobel: expected CV_8UC1 or CV_8UC3");

    // Grayscale source for gradient computation
    if (input.type() == CV_8UC3)
        cv::cvtColor(input, m_gray, cv::COLOR_BGR2GRAY);
    const cv::Mat& gray = (input.type() == CV_8UC1) ? input : m_gray;

    // Sobel gradients in CV_16S to preserve sign across the zero crossing.
    // Magnitude: 0.5*|Gx| + 0.5*|Gy| via scaled convertScaleAbs + add.
    cv::Sobel(gray, m_sobelX, CV_16S, 1, 0);
    cv::Sobel(gray, m_sobelY, CV_16S, 0, 1);
    cv::convertScaleAbs(m_sobelX, m_magX, 0.5);
    cv::convertScaleAbs(m_sobelY, m_magY, 0.5);
    cv::add(m_magX, m_magY, m_mag);


    cv::threshold(m_mag, m_mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    if (!m_overlay)
        return m_mask;
    if (input.type() == CV_8UC1)
        cv::cvtColor(input, m_dst, cv::COLOR_GRAY2BGR);
    else
        input.copyTo(m_dst);
    m_dst.setTo(cv::Scalar(0, 255, 0), m_mask);
    return m_dst;
}
