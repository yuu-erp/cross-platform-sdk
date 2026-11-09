#pragma once

#include "core/application/ports/i_clock.h"

#include <chrono>

namespace fiaisdk::test {

/// Deterministic IClock: time only moves when the test calls advance()/setNow().
/// Not thread-safe; use from one thread.
class FakeClock final : public core::IClock {
  public:
    FakeClock() = default;
    explicit FakeClock(core::Timestamp start) : now_(start) {}

    [[nodiscard]] core::Timestamp now() const noexcept override {
        return now_;
    }
    [[nodiscard]] core::MonotonicTime monotonicNow() const noexcept override {
        return monotonic_;
    }

    void advance(std::chrono::nanoseconds by) {
        now_ += std::chrono::duration_cast<core::Timestamp::duration>(by);
        monotonic_ += std::chrono::duration_cast<core::MonotonicTime::duration>(by);
    }
    void setNow(core::Timestamp now) {
        now_ = now;
    }

  private:
    core::Timestamp now_{};
    core::MonotonicTime monotonic_{};
};

} // namespace fiaisdk::test
