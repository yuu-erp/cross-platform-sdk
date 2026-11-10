#pragma once

#include "core/application/sdk_core.h"
#include "core/domain/result.h"
#include "modules/crypto/api/crypto_facade.h"
#include "modules/network/api/network_facade.h"
#include "modules/wallet/api/wallet_facade.h"

#include <memory>

namespace fiaisdk::bootstrap {

/// Non-owning pointers to module facades, for the bindings. Valid while `core` is alive
/// (the modules own the facades; they are released during SdkCore destruction).
struct ModuleApis {
    crypto::CryptoFacade *crypto = nullptr;
    wallet::WalletFacade *wallet = nullptr;
    network::NetworkFacade *network = nullptr;
};

/// A fully wired SDK instance as produced by the composition root.
struct ComposedSdk {
    std::unique_ptr<core::SdkCore> core;
    ModuleApis apis;
};

/// Builds a fully wired SdkCore: default core adapters + all built-in modules with their
/// adapters. This is the ONLY place that knows concrete module and adapter types.
[[nodiscard]] core::Result<ComposedSdk> composeSdk(core::SdkConfig config);

} // namespace fiaisdk::bootstrap
