#pragma once
#include <stdexcept>
#include <string>

namespace FPGAlix {

class ExceptionNotImplemented : public std::logic_error {
public:
    explicit ExceptionNotImplemented(const std::string &msg = "not implemented")
        : std::logic_error(msg) {}
};

} // namespace FPGAlix
