#pragma once
#include "modules/crypto/application/key_validation.h"
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::crypto {
class VerifySignature {
  public:
    explicit VerifySignature(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<bool> execute(KeyScheme scheme,
                                             std::span<const std::uint8_t> publicKey,
                                             std::span<const std::uint8_t> data,
                                             std::span<const std::uint8_t> signature) const {
        if (!validScheme(scheme) || publicKey.size() != publicKeySize(scheme) ||
            signature.size() != signatureSize(scheme) || !validSigningData(scheme, data.size()))
            return core::Error::invalidArgument("invalid verification scheme or input length");
        return provider_->verify(scheme, publicKey, data, signature);
    }

  private:
    const ICryptoProvider *provider_;
};
} // namespace fiaisdk::crypto
