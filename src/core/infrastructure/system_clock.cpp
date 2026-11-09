#include "core/infrastructure/system_clock.h"

#include <chrono>

namespace fiaisdk::core {

Timestamp SystemClock::now() const noexcept {
    return std::chrono::system_clock::now();
}

MonotonicTime SystemClock::monotonicNow() const noexcept {
    return std::chrono::steady_clock::now();
}

} // namespace fiaisdk::core
