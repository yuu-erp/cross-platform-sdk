#pragma once

#include "modules/crypto/application/ports/i_crypto_provider.h"

#include <memory>

namespace fiaisdk::crypto {

class LibraryCryptoProvider final : public ICryptoProvider {
  public:
    LibraryCryptoProvider();
    ~LibraryCryptoProvider() override;
    [[nodiscard]] core::Result<Sha256Digest>
    sha256(std::span<const std::uint8_t> input) const override;
    [[nodiscard]] core::Result<void> privateRandom(std::span<std::uint8_t> output) const override;

    [[nodiscard]] core::Result<Sha256Digest>
    keccak256(std::span<const std::uint8_t> input) const override;
    [[nodiscard]] core::Result<core::SecureBytes>
    generatePrivateKey(KeyScheme scheme) const override;
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    derivePublicKey(KeyScheme scheme, std::span<const std::uint8_t> secret) const override;
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    sign(KeyScheme scheme, std::span<const std::uint8_t> secret,
         std::span<const std::uint8_t> data) const override;
    [[nodiscard]] core::Result<bool> verify(KeyScheme scheme,
                                            std::span<const std::uint8_t> publicKey,
                                            std::span<const std::uint8_t> data,
                                            std::span<const std::uint8_t> signature) const override;
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    aeadSeal(std::span<const std::uint8_t, kAeadKeySize> key, std::span<const std::uint8_t> aad,
             std::span<const std::uint8_t> plaintext) const override;
    [[nodiscard]] core::Result<core::SecureBytes>
    aeadOpen(std::span<const std::uint8_t, kAeadKeySize> key, std::span<const std::uint8_t> aad,
             std::span<const std::uint8_t> sealed) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fiaisdk::crypto
