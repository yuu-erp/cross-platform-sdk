#include "core/composition_root.h"

#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "core/infrastructure/system_clock.h"
#include "modules/network/api/network_module.h"
#include "modules/smc/api/smc_module.h"
#include "modules/wallet/api/wallet_module.h"

#include <utility>

namespace fiaisdk::core {

Result<std::unique_ptr<SdkCore>> composeSdk(SdkConfig config) {
    CoreServices services;
    services.clock = std::make_unique<SystemClock>();
    services.logger = std::make_unique<NullLogger>(); // until the logging ADR
    services.eventBus = std::make_unique<InProcessEventBus>(*services.logger);

    ModuleRegistry modules;
    // Registration order = initialisation order; register dependencies first
    // (docs/architecture/module-graph.md).
    if (auto added = modules.add(std::make_unique<network::NetworkModule>()); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<wallet::WalletModule>()); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<smc::SmcModule>()); !added) {
        return added.error();
    }

    return SdkCore::create(std::move(config), std::move(services), std::move(modules));
}

} // namespace fiaisdk::core
