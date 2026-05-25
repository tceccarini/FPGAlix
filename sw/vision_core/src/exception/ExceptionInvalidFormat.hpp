#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionInvalidFormat : public std::runtime_error {
public:
    explicit ExceptionInvalidFormat(const std::string &msg = "Invalid pixel format")
        : std::runtime_error(msg) {}
};

} // namespace FPGAlix
