#pragma once

#include "core/sdk_core.h"
#include "kernel/result.h"

#include <memory>

namespace fiaisdk::core {

/// Builds a fully wired SdkCore with all built-in modules and their concrete adapters.
/// This is the ONLY place that knows concrete module/infrastructure types.
[[nodiscard]] kernel::Result<std::unique_ptr<SdkCore>> composeSdk(SdkConfig config);

} // namespace fiaisdk::core
