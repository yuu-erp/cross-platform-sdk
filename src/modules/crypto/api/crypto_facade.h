#pragma once

#include "modules/crypto/application/derive_public_key.h"
#include "modules/crypto/application/generate_private_key.h"
#include "modules/crypto/application/generate_random_bytes.h"
#include "modules/crypto/application/hash_keccak256.h"
#include "modules/crypto/application/hash_sha256.h"
#include "modules/crypto/application/sign_data.h"
#include "modules/crypto/application/verify_signature.h"

namespace fiaisdk::crypto {

/// Stateless, thread-safe crypto operations. The injected provider must outlive this facade.
class CryptoFacade {
  public:
    explicit CryptoFacade(const ICryptoProvider &provider) noexcept
        : hash_(provider), random_(provider), keccak_(provider), generateKey_(provider),
          deriveKey_(provider), sign_(provider), verify_(provider) {}
    [[nodiscard]] core::Result<Sha256Digest> sha256(std::span<const std::uint8_t> input) const {
        return hash_.execute(input);
    }
    [[nodiscard]] core::Result<core::SecureBytes> randomBytes(std::size_t size) const {
        return random_.execute(size);
    }

    [[nodiscard]] core::Result<Sha256Digest> keccak256(std::span<const std::uint8_t> data) const {
        return keccak_.execute(data);
    }
    [[nodiscard]] core::Result<core::SecureBytes> generatePrivateKey(KeyScheme scheme) const {
        return generateKey_.execute(scheme);
    }
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    derivePublicKey(KeyScheme scheme, std::span<const std::uint8_t> secret) const {
        return deriveKey_.execute(scheme, secret);
    }
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    sign(KeyScheme scheme, std::span<const std::uint8_t> secret,
         std::span<const std::uint8_t> data) const {
        return sign_.execute(scheme, secret, data);
    }
    [[nodiscard]] core::Result<bool> verify(KeyScheme scheme,
                                            std::span<const std::uint8_t> publicKey,
                                            std::span<const std::uint8_t> data,
                                            std::span<const std::uint8_t> signature) const {
        return verify_.execute(scheme, publicKey, data, signature);
    }

  private:
    HashSha256 hash_;
    GenerateRandomBytes random_;
    HashKeccak256 keccak_;
    GeneratePrivateKey generateKey_;
    DerivePublicKey deriveKey_;
    SignData sign_;
    VerifySignature verify_;
};

} // namespace fiaisdk::crypto
