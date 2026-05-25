#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionPoolExhausted : public std::runtime_error {
public:
    explicit ExceptionPoolExhausted(const std::string &msg = "FrameBuffer: pool exhausted")
        : std::runtime_error(msg) {}
};

} // namespace FPGAlix
