#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionQueueEmpty : public std::runtime_error {
public:
    explicit ExceptionQueueEmpty(const std::string &msg = "FrameBuffer: queue is empty")
        : std::runtime_error(msg) {}
};

} // namespace FPGAlix
