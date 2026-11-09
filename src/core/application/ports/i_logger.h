#pragma once

#include <cstdint>
#include <string_view>

namespace fiaisdk::core {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warning, Error };

/// One log line. Views are only valid during ILogger::write; copy them if kept.
struct LogRecord {
    LogLevel level = LogLevel::Info;
    /// Source of the message, e.g. "core", "wallet". String literal recommended.
    std::string_view category;
    /// Never contains secrets (keys, seeds, mnemonics, tokens, passwords) or personal data.
    std::string_view message;
};

/// Port: log sink. Implementations must be thread-safe and must not throw.
/// Prefer the Logger helper (core/application/logger.h) over calling this directly.
class ILogger {
  public:
    ILogger() = default;
    virtual ~ILogger() = default;
    ILogger(const ILogger &) = delete;
    ILogger &operator=(const ILogger &) = delete;
    ILogger(ILogger &&) = delete;
    ILogger &operator=(ILogger &&) = delete;

    /// Cheap check so callers can skip building expensive messages.
    [[nodiscard]] virtual bool isEnabled(LogLevel level) const noexcept = 0;
    virtual void write(const LogRecord &record) noexcept = 0;
};

} // namespace fiaisdk::core
