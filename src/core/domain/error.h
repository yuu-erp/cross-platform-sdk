#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace fiaisdk::core {

/// Error categories shared by every module. Mapped to fiaisdk_status only in src/bindings.
/// Add a category only when callers must react to it differently; otherwise use Internal.
enum class ErrorCode : std::uint8_t {
    InvalidArgument,
    NotInitialized,
    ModuleNotFound,
    ModuleInitFailed,
    Internal,
};

/// Failure value carried by Result<T>.
struct Error {
    ErrorCode code;
    /// Developer-facing context. Must never contain secrets (keys, seeds, tokens) or user data.
    std::string message;

    Error(ErrorCode errorCode, std::string text) : code(errorCode), message(std::move(text)) {}

    [[nodiscard]] static Error invalidArgument(std::string text) {
        return {ErrorCode::InvalidArgument, std::move(text)};
    }
    [[nodiscard]] static Error notInitialized(std::string text) {
        return {ErrorCode::NotInitialized, std::move(text)};
    }
    [[nodiscard]] static Error internal(std::string text) {
        return {ErrorCode::Internal, std::move(text)};
    }
};

} // namespace fiaisdk::core
