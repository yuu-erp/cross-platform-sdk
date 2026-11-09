#pragma once

#include "core/application/ports/i_logger.h"

namespace fiaisdk::core {

/// ILogger that discards everything. Default until the logging ADR defines how logs reach the
/// host application (e.g. a C callback registered through the ABI).
class NullLogger final : public ILogger {
  public:
    [[nodiscard]] bool isEnabled(LogLevel /*level*/) const noexcept override {
        return false;
    }
    void write(const LogRecord & /*record*/) noexcept override {}
};

} // namespace fiaisdk::core
