#pragma once

#include "core/application/ports/i_clock.h"

namespace fiaisdk::core {

/// IClock backed by std::chrono::system_clock / steady_clock. Thread-safe, stateless.
class SystemClock final : public IClock {
  public:
    [[nodiscard]] Timestamp now() const noexcept override;
    [[nodiscard]] MonotonicTime monotonicNow() const noexcept override;
};

} // namespace fiaisdk::core
