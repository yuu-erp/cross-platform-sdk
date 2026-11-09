#pragma once

#include "core/application/sdk_core.h"
#include "core/domain/result.h"
#include "modules/network/api/network_facade.h"

#include <memory>

namespace fiaisdk::core {

/// Non-owning pointers to module facades, for the bindings. Valid while `core` is alive
/// (the modules own the facades; they are released during SdkCore destruction).
struct ModuleApis {
    network::NetworkFacade *network = nullptr;
};

/// A fully wired SDK instance as produced by the composition root.
struct ComposedSdk {
    std::unique_ptr<SdkCore> core;
    ModuleApis apis;
};

/// Builds a fully wired SdkCore: default core adapters + all built-in modules with their
/// adapters. This is the ONLY place that knows concrete module and adapter types.
[[nodiscard]] Result<ComposedSdk> composeSdk(SdkConfig config);

} // namespace fiaisdk::core
