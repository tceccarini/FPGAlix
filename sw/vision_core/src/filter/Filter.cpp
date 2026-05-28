#include "Filter.hpp"
#include "exception/ExceptionNotImplemented.hpp"

uint32_t Filter::getId() const {
    return m_filterId;
}

void Filter::filter(cv::Mat& /*input*/, cv::Mat* /*output*/) {
    throw FPGAlix::ExceptionNotImplemented("Filter: pointer overload not implemented");
}
