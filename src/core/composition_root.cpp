#include "core/composition_root.h"

#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "core/infrastructure/system_clock.h"
#include "modules/network/api/network_module.h"
#include "modules/network/infrastructure/simulated_node_probe.h"
#include "modules/smc/api/smc_module.h"
#include "modules/wallet/api/wallet_module.h"

#include <utility>

namespace fiaisdk::core {

Result<ComposedSdk> composeSdk(SdkConfig config) {
    CoreServices services;
    services.clock = std::make_unique<SystemClock>();
    services.logger = std::make_unique<NullLogger>(); // until the logging ADR
    services.eventBus = std::make_unique<InProcessEventBus>(*services.logger);

    // DEMO transport: replace SimulatedNodeProbe with the real HTTP/WebSocket probe once the
    // transport ADR is decided. Nothing else changes.
    auto networkModule =
        std::make_unique<network::NetworkModule>(std::make_unique<network::SimulatedNodeProbe>());
    network::NetworkModule *network = networkModule.get(); // owned by the registry below

    ModuleRegistry modules;
    // Registration order = initialisation order; register dependencies first
    // (docs/architecture/module-graph.md).
    if (auto added = modules.add(std::move(networkModule)); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<wallet::WalletModule>()); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<smc::SmcModule>()); !added) {
        return added.error();
    }

    auto core = SdkCore::create(std::move(config), std::move(services), std::move(modules));
    if (!core) {
        return core.error();
    }

    ComposedSdk composed;
    composed.core = std::move(core).value();
    composed.apis.network = network->facade(); // valid: modules are initialised now
    return composed;
}

} // namespace fiaisdk::core
