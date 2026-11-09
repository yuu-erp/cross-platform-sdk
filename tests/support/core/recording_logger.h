#pragma once

#include "core/application/ports/i_logger.h"

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace fiaisdk::test {

/// ILogger that keeps copies of every record at or above `minLevel`. Thread-safe.
class RecordingLogger final : public core::ILogger {
  public:
    struct Entry {
        core::LogLevel level;
        std::string category;
        std::string message;
    };

    explicit RecordingLogger(core::LogLevel minLevel = core::LogLevel::Trace)
        : minLevel_(minLevel) {}

    [[nodiscard]] bool isEnabled(core::LogLevel level) const noexcept override {
        return level >= minLevel_;
    }

    void write(const core::LogRecord &record) noexcept override {
        try {
            const std::scoped_lock lock(mutex_);
            entries_.push_back(
                {record.level, std::string(record.category), std::string(record.message)});
        } catch (...) { // a logger must never throw
        }
    }

    [[nodiscard]] std::vector<Entry> entries() const {
        const std::scoped_lock lock(mutex_);
        return entries_;
    }

  private:
    core::LogLevel minLevel_;
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
};

} // namespace fiaisdk::test
