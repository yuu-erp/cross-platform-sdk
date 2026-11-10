#pragma once

#include "core/domain/result.h"
#include "core/domain/secure_bytes.h"
#include "modules/crypto/domain/key_scheme.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace fiaisdk::crypto {

using Sha256Digest = std::array<std::uint8_t, 32>;

/// Primitive provider boundary. Implementations must be thread-safe; errors contain no secrets.
class ICryptoProvider {
  public:
    virtual ~ICryptoProvider() = default;
    [[nodiscard]] virtual core::Result<Sha256Digest>
    sha256(std::span<const std::uint8_t> input) const = 0;
    [[nodiscard]] virtual core::Result<Sha256Digest>
    keccak256(std::span<const std::uint8_t> input) const = 0;
    [[nodiscard]] virtual core::Result<core::SecureBytes>
    generatePrivateKey(KeyScheme scheme) const = 0;
    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>>
    derivePublicKey(KeyScheme scheme, std::span<const std::uint8_t> secret) const = 0;
    [[nodiscard]] virtual core::Result<std::vector<std::uint8_t>>
    sign(KeyScheme scheme, std::span<const std::uint8_t> secret,
         std::span<const std::uint8_t> data) const = 0;
    [[nodiscard]] virtual core::Result<bool>
    verify(KeyScheme scheme, std::span<const std::uint8_t> publicKey,
           std::span<const std::uint8_t> data, std::span<const std::uint8_t> signature) const = 0;
    /// On failure the caller must discard/wipe the entire output.
    [[nodiscard]] virtual core::Result<void>
    privateRandom(std::span<std::uint8_t> output) const = 0;
};

} // namespace fiaisdk::crypto
