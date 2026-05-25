#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionQueueFull : public std::runtime_error {
public:
    explicit ExceptionQueueFull(const std::string &msg = "FrameBuffer: queue is full")
        : std::runtime_error(msg) {}
};

} // namespace FPGAlix
