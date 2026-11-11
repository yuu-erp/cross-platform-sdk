#pragma once

#include "modules/crypto/application/ports/i_crypto_provider.h"

#include <cstddef>

namespace fiaisdk::crypto {

/// Authenticated encryption of small secrets (AES-256-GCM). Size limits keep int conversions safe.
class SealData {
  public:
    static constexpr std::size_t kMaxSize = 1U << 20U;
    explicit SealData(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    execute(std::span<const std::uint8_t, kAeadKeySize> key, std::span<const std::uint8_t> aad,
            std::span<const std::uint8_t> plaintext) const {
        if (plaintext.size() > kMaxSize || aad.size() > kMaxSize) {
            return core::Error::invalidArgument("AEAD input is too large");
        }
        return provider_->aeadSeal(key, aad, plaintext);
    }

  private:
    const ICryptoProvider *provider_;
};

class OpenSealedData {
  public:
    explicit OpenSealedData(const ICryptoProvider &provider) noexcept : provider_(&provider) {}
    [[nodiscard]] core::Result<core::SecureBytes>
    execute(std::span<const std::uint8_t, kAeadKeySize> key, std::span<const std::uint8_t> aad,
            std::span<const std::uint8_t> sealed) const {
        if (sealed.size() < kAeadOverhead || sealed.size() > SealData::kMaxSize + kAeadOverhead ||
            aad.size() > SealData::kMaxSize) {
            return core::Error::invalidArgument("invalid AEAD sealed size");
        }
        return provider_->aeadOpen(key, aad, sealed);
    }

  private:
    const ICryptoProvider *provider_;
};

} // namespace fiaisdk::crypto
