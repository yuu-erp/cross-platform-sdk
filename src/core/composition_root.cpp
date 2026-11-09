#include "core/composition_root.h"

#include "modules/network/api/network_module.h"
#include "modules/smc/api/smc_module.h"
#include "modules/wallet/api/wallet_module.h"

#include <utility>

namespace fiaisdk::core {

kernel::Result<std::unique_ptr<SdkCore>> composeSdk(SdkConfig config) {
    ModuleRegistry modules;

    // Registration order = initialisation order; register dependencies first.
    if (auto added = modules.add(std::make_unique<network::NetworkModule>()); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<wallet::WalletModule>()); !added) {
        return added.error();
    }
    if (auto added = modules.add(std::make_unique<smc::SmcModule>()); !added) {
        return added.error();
    }

    return SdkCore::create(std::move(config), std::move(modules));
}

} // namespace fiaisdk::core
