#pragma once
#include "modules/crypto/application/ports/i_crypto_provider.h"
namespace fiaisdk::test {
class UnavailableCryptoProvider : public crypto::ICryptoProvider {
  public:
    core::Result<crypto::Sha256Digest> sha256(std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<crypto::Sha256Digest> keccak256(std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<void> privateRandom(std::span<std::uint8_t>) const override {
        return failure();
    }
    core::Result<core::SecureBytes> generatePrivateKey(crypto::KeyScheme) const override {
        return failure();
    }
    core::Result<std::vector<std::uint8_t>>
    derivePublicKey(crypto::KeyScheme, std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<std::vector<std::uint8_t>> sign(crypto::KeyScheme, std::span<const std::uint8_t>,
                                                 std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<bool> verify(crypto::KeyScheme, std::span<const std::uint8_t>,
                              std::span<const std::uint8_t>,
                              std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<std::vector<std::uint8_t>>
    aeadSeal(std::span<const std::uint8_t, crypto::kAeadKeySize>, std::span<const std::uint8_t>,
             std::span<const std::uint8_t>) const override {
        return failure();
    }
    core::Result<core::SecureBytes> aeadOpen(std::span<const std::uint8_t, crypto::kAeadKeySize>,
                                             std::span<const std::uint8_t>,
                                             std::span<const std::uint8_t>) const override {
        return failure();
    }

  private:
    static core::Error failure() {
        return core::Error::unavailable("test provider unavailable");
    }
};
} // namespace fiaisdk::test
