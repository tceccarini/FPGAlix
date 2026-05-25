#include "Frame.hpp"

namespace FPGAlix {

Frame::Frame(int width, int height, int type) {
    m_frameData = cv::Mat(height, width, type);
    m_isBusy = false;
}

Frame::~Frame() {
    m_isBusy.store(true);
    m_frameData.release();
}

cv::Mat &Frame::mat() {
    return m_frameData;
}

const cv::Mat &Frame::mat() const {
    return m_frameData;
}

bool Frame::isBusy() const  { 
    return m_isBusy.load();  
}

void Frame::setBusy() { 
    m_isBusy.store(true);    
}

void Frame::clearBusy() {
    m_isBusy.store(false);   
}

} // namespace FPGAlix
