#pragma once

#include "core/domain/error.h"

#include <fiaisdk/types.h>

namespace fiaisdk::bindings {

/// Single mapping point from internal errors to public status codes.
[[nodiscard]] fiaisdk_status toStatus(const core::Error &error) noexcept;

/// Map the exception currently being handled. Call only inside a catch block.
[[nodiscard]] fiaisdk_status statusFromCurrentException() noexcept;

} // namespace fiaisdk::bindings
