#pragma once

#include "core/application/sdk_core.h"
#include "core/domain/result.h"

#include <memory>

namespace fiaisdk::core {

/// Builds a fully wired SdkCore: default core adapters + all built-in modules.
/// This is the ONLY place that knows concrete module and adapter types.
[[nodiscard]] Result<std::unique_ptr<SdkCore>> composeSdk(SdkConfig config);

} // namespace fiaisdk::core
