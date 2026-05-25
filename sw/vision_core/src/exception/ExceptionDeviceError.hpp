#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionDeviceError : public std::runtime_error {
public:
    explicit ExceptionDeviceError(const std::string &msg = "Capturer: failed to open video device")
        : std::runtime_error(msg) {}
};

} // namespace FPGAlix
