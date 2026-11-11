#include "bootstrap/composition_root.h"

#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "core/infrastructure/system_clock.h"
#include "modules/crypto/api/crypto_module.h"
#include "modules/crypto/infrastructure/library_crypto_provider.h"
#include "modules/network/api/network_module.h"
#include "modules/network/infrastructure/tcp_node_connector.h"
#include "modules/smc/api/smc_module.h"
#include "modules/wallet/api/wallet_module.h"
#include "modules/wallet/infrastructure/sqlite_wallet_store.h"

#include <utility>

namespace fiaisdk::bootstrap {

namespace {

/// Crypto owned by one wallet store, so a store does not depend on any SDK's lifetime.
struct StoreCrypto {
    crypto::LibraryCryptoProvider provider;
    crypto::CryptoFacade facade{provider};
};

} // namespace

core::Result<std::unique_ptr<wallet::IWalletStore>>
openWalletStore(const char *path, std::span<const std::uint8_t, 32> key) {
    // Created once per opened store (not per operation); throws like composeSdk on failure.
    auto owner = std::make_shared<StoreCrypto>();
    std::shared_ptr<const crypto::CryptoFacade> facade(owner, &owner->facade);
    return wallet::openSqliteWalletStore(path, key, std::move(facade));
}

core::Result<ComposedSdk> composeSdk(core::SdkConfig config) {
    core::CoreServices services;
    services.clock = std::make_unique<core::SystemClock>();
    services.logger = std::make_unique<core::NullLogger>(); // until the logging ADR
    services.eventBus = std::make_unique<core::InProcessEventBus>(*services.logger);

    auto networkModule =
        std::make_unique<network::NetworkModule>(std::make_unique<network::TcpNodeConnector>());
    network::NetworkModule *network = networkModule.get(); // owned by the registry below

    auto cryptoModule =
        std::make_unique<crypto::CryptoModule>(std::make_unique<crypto::LibraryCryptoProvider>());
    crypto::CryptoModule *crypto = cryptoModule.get();

    core::ModuleRegistry modules;
    if (auto added = modules.add(std::move(cryptoModule)); !added) {
        return added.error();
    }
    // Registration order = initialisation order; register dependencies first
    // (docs/architecture/module-graph.md).
    if (auto added = modules.add(std::move(networkModule)); !added) {
        return added.error();
    }
    // Wallet requires the initialized crypto facade. The registry initializes in registration
    // order and shuts down in reverse, so crypto must be registered before wallet.
    auto walletModule = std::make_unique<wallet::WalletModule>(*crypto);
    wallet::WalletModule *wallet = walletModule.get();
    if (auto added = modules.add(std::move(walletModule)); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<smc::SmcModule>()); !added) {
        return added.error();
    }

    auto core = core::SdkCore::create(std::move(config), std::move(services), std::move(modules));
    if (!core) {
        return core.error();
    }

    ComposedSdk composed;
    composed.core = std::move(core).value();
    composed.apis.crypto = crypto->facade();
    composed.apis.wallet = wallet->facade();
    composed.apis.network = network->facade(); // valid: modules are initialised now
    return composed;
}

} // namespace fiaisdk::bootstrap
