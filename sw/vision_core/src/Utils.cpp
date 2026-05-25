#include "Utils.hpp"
#include <chrono>
#include <cstdio>
#include <unordered_map>
#include <mutex>

namespace FPGAlix {

static void rprint_impl(int interval_ms, const char *fmt, va_list args) {
    using Clock = std::chrono::steady_clock;
    static std::unordered_map<const char *, Clock::time_point> s_last;
    static std::mutex s_mutex;

    auto now = Clock::now();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto &t = s_last[fmt];
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count() < interval_ms)
            return;
        t = now;
    }
    vfprintf(stderr, fmt, args);
}

void rprint(int interval_ms, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rprint_impl(interval_ms, fmt, args);
    va_end(args);
}

void rprint(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rprint_impl(1000, fmt, args);
    va_end(args);
}

} // namespace FPGAlix
