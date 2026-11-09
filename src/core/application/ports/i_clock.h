#pragma once

#include "core/domain/timestamp.h"

namespace fiaisdk::core {

/// Port: source of the current time. Inject it instead of calling std::chrono clocks so that
/// time-dependent logic (expiry, timeouts, event timestamps) is testable with a fake clock.
class IClock {
  public:
    IClock() = default;
    virtual ~IClock() = default;
    IClock(const IClock &) = delete;
    IClock &operator=(const IClock &) = delete;
    IClock(IClock &&) = delete;
    IClock &operator=(IClock &&) = delete;

    /// Wall-clock time (UTC). May jump (NTP, user changes).
    [[nodiscard]] virtual Timestamp now() const noexcept = 0;
    /// Monotonic time for durations and timeouts. Never jumps backwards.
    [[nodiscard]] virtual MonotonicTime monotonicNow() const noexcept = 0;
};

} // namespace fiaisdk::core
