#pragma once
#include "Filter.hpp"
#include <opencv2/imgproc.hpp>

// Sobel edge detection filter. Accepts CV_8UC1 or CV_8UC3 input.
//
// overlay=false: returns the raw edge magnitude (CV_8UC1, 0.5*|Gx|+0.5*|Gy|),
//                no color conversion — same depth as the internal computation.
// overlay=true:  returns a CV_8UC3 BGR frame with edges painted green (0,255,0).
//                Grayscale input is converted to BGR before painting.
//                Edge mask computed via Otsu thresholding on the magnitude.
class FilterSobel : public Filter {
public:
    explicit FilterSobel(bool overlay);

    // preserveInput is ignored: the filter never modifies input in place.
    cv::Mat& filter(cv::Mat& input, bool preserveInput) override;

private:
    bool    m_overlay;
    cv::Mat m_gray;   // grayscale intermediate (CV_8UC1)
    cv::Mat m_sobelX; // horizontal Sobel gradient (CV_16S)
    cv::Mat m_sobelY; // vertical Sobel gradient (CV_16S)
    cv::Mat m_magX;   // 0.5*|Gx| (CV_8U)
    cv::Mat m_magY;   // 0.5*|Gy| (CV_8U)
    cv::Mat m_mag;    // combined edge magnitude: 0.5*|Gx| + 0.5*|Gy| (CV_8UC1) — returned in non-overlay mode
    cv::Mat m_mask;   // Otsu binary edge mask (CV_8UC1) — overlay mode only
    cv::Mat m_dst;    // BGR output buffer — overlay mode only
};
