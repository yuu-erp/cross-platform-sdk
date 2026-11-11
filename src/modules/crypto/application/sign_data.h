#pragma once
#include "modules/crypto/application/key_validation.h"
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::crypto {
class SignData {
  public:
    explicit SignData(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    execute(KeyScheme scheme, std::span<const std::uint8_t> secret,
            std::span<const std::uint8_t> data) const {
        if (!validScheme(scheme) || secret.size() != 32 || !validSigningData(scheme, data.size())) {
            return core::Error::invalidArgument(
                "invalid signing scheme, key length or digest length");
        }
        return provider_->sign(scheme, secret, data);
    }

  private:
    const ICryptoProvider *provider_;
};
} // namespace fiaisdk::crypto
