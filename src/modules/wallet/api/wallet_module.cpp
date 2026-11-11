#include "modules/wallet/api/wallet_module.h"
namespace fiaisdk::wallet {
core::Result<void> WalletModule::initialize(const core::ModuleContext &context) {
    static_cast<void>(context);
    const auto *crypto = crypto_->facade();
    if (crypto == nullptr) {
        return core::Error::notInitialized("wallet requires an initialized crypto module");
    }
    facade_ = std::make_unique<WalletFacade>(*crypto);
    return {};
}
void WalletModule::shutdown() noexcept {
    facade_.reset();
}
} // namespace fiaisdk::wallet
