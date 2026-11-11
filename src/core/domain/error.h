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
    /// A required external resource (node, service, device) cannot be reached or timed out.
    Unavailable,
    Internal,
    /// The requested record does not exist.
    NotFound,
    /// A record with the same identity already exists.
    AlreadyExists,
    /// A key or credential was rejected (authentication tag or verifier mismatch).
    AuthenticationFailed,
};

/// Failure value carried by Result<T>.
// The NOLINT silences a clang-analyzer false positive: it cannot model std::variant inside
// Result and reports the implicit copy of an Error held in the variant as uninitialised.
struct Error { // NOLINT(clang-analyzer-core.uninitialized.Assign)
    ErrorCode code = ErrorCode::Internal;
    /// Developer-facing context. Must never contain secrets (keys, seeds, tokens) or user data.
    std::string message;

    Error(ErrorCode errorCode, std::string text) : code(errorCode), message(std::move(text)) {}

    [[nodiscard]] static Error invalidArgument(std::string text) {
        return {ErrorCode::InvalidArgument, std::move(text)};
    }
    [[nodiscard]] static Error notInitialized(std::string text) {
        return {ErrorCode::NotInitialized, std::move(text)};
    }
    [[nodiscard]] static Error unavailable(std::string text) {
        return {ErrorCode::Unavailable, std::move(text)};
    }
    [[nodiscard]] static Error internal(std::string text) {
        return {ErrorCode::Internal, std::move(text)};
    }
};

} // namespace fiaisdk::core
