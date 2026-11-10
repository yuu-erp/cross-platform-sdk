#include "modules/wallet/api/wallet_module.h"
namespace fiaisdk::wallet {
core::Result<void> WalletModule::initialize(const core::ModuleContext &context) {
    static_cast<void>(context);
    facade_ = std::make_unique<WalletFacade>(*crypto_);
    return {};
}
void WalletModule::shutdown() noexcept {
    facade_.reset();
}
} // namespace fiaisdk::wallet
