#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace fiaisdk::kernel {

/// Internal error categories shared by all modules. Mapped to fiaisdk_status only in bindings.
enum class ErrorCode : std::uint8_t {
    InvalidArgument,
    NotInitialized,
    ModuleNotFound,
    ModuleInitFailed,
    Internal,
};

struct Error {
    ErrorCode code = ErrorCode::Internal;
    /// Developer-facing context. Must never contain secrets or user data.
    std::string message;

    Error(ErrorCode errorCode, std::string text) : code(errorCode), message(std::move(text)) {}
};

} // namespace fiaisdk::kernel
