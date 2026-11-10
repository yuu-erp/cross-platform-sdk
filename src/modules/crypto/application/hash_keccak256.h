#pragma once
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::crypto {
class HashKeccak256 {
  public:
    explicit HashKeccak256(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<Sha256Digest> execute(std::span<const std::uint8_t> input) const {
        return provider_->keccak256(input);
    }

  private:
    const ICryptoProvider *provider_;
};
} // namespace fiaisdk::crypto
