#pragma once
#include "modules/crypto/application/key_validation.h"
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::crypto {
class DerivePublicKey {
  public:
    explicit DerivePublicKey(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    execute(KeyScheme scheme, std::span<const std::uint8_t> secret) const {
        if (!validScheme(scheme) || secret.size() != 32) {
            return core::Error::invalidArgument("key scheme or private key length is invalid");
        }
        return provider_->derivePublicKey(scheme, secret);
    }

  private:
    const ICryptoProvider *provider_;
};
} // namespace fiaisdk::crypto
