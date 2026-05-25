#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionOutOfRange : public std::out_of_range {
public:
    explicit ExceptionOutOfRange(const std::string &msg = "index out of range")
        : std::out_of_range(msg) {}
};

} // namespace FPGAlix
