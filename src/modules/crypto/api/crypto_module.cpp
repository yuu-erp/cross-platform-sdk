#include "modules/crypto/api/crypto_module.h"

#include <utility>

namespace fiaisdk::crypto {

CryptoModule::CryptoModule(std::unique_ptr<ICryptoProvider> provider)
    : provider_(std::move(provider)) {}

core::Result<void> CryptoModule::initialize(const core::ModuleContext &context) {
    static_cast<void>(context);
    if (!provider_) {
        return core::Error::invalidArgument("crypto module needs a provider");
    }
    facade_ = std::make_unique<CryptoFacade>(*provider_);
    return {};
}

void CryptoModule::shutdown() noexcept {
    facade_.reset();
}

} // namespace fiaisdk::crypto
