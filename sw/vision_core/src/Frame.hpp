#pragma once
#include <atomic>
#include <opencv2/opencv.hpp>

namespace FPGAlix {

class Frame {
public:
    Frame(int width, int height, int type);
    virtual ~Frame();

    bool     isBusy() const;
    void     setBusy();
    void     clearBusy();
    cv::Mat       &mat();
    const cv::Mat &mat() const;
private:
    std::atomic<bool> m_isBusy{false};
    cv::Mat m_frameData;
};

} // namespace FPGAlix
