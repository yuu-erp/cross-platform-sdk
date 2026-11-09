#pragma once

#include <chrono>

namespace fiaisdk::core {

/// Wall-clock time (UTC), for values that are stored or shown: event times, expiries.
using Timestamp = std::chrono::system_clock::time_point;

/// Monotonic time, for measuring durations and timeouts. Never compare across processes.
using MonotonicTime = std::chrono::steady_clock::time_point;

/// Domain code never reads a clock directly: the current time comes from the IClock port
/// (core/application/ports/i_clock.h) so it can be faked in tests.

} // namespace fiaisdk::core
