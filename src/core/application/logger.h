#pragma once

#include "core/application/ports/i_logger.h"

#include <string_view>

namespace fiaisdk::core {

/// Convenience front-end over ILogger with a fixed category:
///
///     Logger log(context.logger(), "wallet");
///     log.info("wallet created");
///
/// `category` must outlive the Logger (use a string literal). Never log secrets.
class Logger {
  public:
    Logger(ILogger &sink, std::string_view category) noexcept : sink_(&sink), category_(category) {}

    void trace(std::string_view message) const noexcept {
        log(LogLevel::Trace, message);
    }
    void debug(std::string_view message) const noexcept {
        log(LogLevel::Debug, message);
    }
    void info(std::string_view message) const noexcept {
        log(LogLevel::Info, message);
    }
    void warning(std::string_view message) const noexcept {
        log(LogLevel::Warning, message);
    }
    void error(std::string_view message) const noexcept {
        log(LogLevel::Error, message);
    }

    [[nodiscard]] bool isEnabled(LogLevel level) const noexcept {
        return sink_->isEnabled(level);
    }

    void log(LogLevel level, std::string_view message) const noexcept {
        if (sink_->isEnabled(level)) {
            sink_->write(LogRecord{level, category_, message});
        }
    }

  private:
    ILogger *sink_;
    std::string_view category_;
};

} // namespace fiaisdk::core
