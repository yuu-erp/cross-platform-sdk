#pragma once
#include "modules/crypto/application/key_validation.h"
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::crypto {
class GeneratePrivateKey {
  public:
    explicit GeneratePrivateKey(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<core::SecureBytes> execute(KeyScheme scheme) const {
        if (!validScheme(scheme))
            return core::Error::invalidArgument("unsupported key scheme");
        return provider_->generatePrivateKey(scheme);
    }

  private:
    const ICryptoProvider *provider_;
};
} // namespace fiaisdk::crypto
