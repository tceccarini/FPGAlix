#pragma once
#include <cstdarg>

namespace FPGAlix {

/* Rate-limited print to stderr, keyed per unique format string literal.
   rprint("msg\n")         — default 1000 ms interval
   rprint(500, "msg\n")    — custom interval in ms */
__attribute__((format(printf, 2, 3))) void rprint(int interval_ms, const char *fmt, ...);
__attribute__((format(printf, 1, 2))) void rprint(const char *fmt, ...);

} // namespace FPGAlix
