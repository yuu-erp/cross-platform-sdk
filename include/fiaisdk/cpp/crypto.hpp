// SPDX-License-Identifier: UNLICENSED
#pragma once

#include <fiaisdk/cpp/sdk.hpp>
#include <fiaisdk/crypto.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fiaisdk {

/// Non-owning view of the SDK's crypto module; must not outlive its SDK handle.
class Crypto {
  public:
    using Sha256Digest = std::array<std::uint8_t, FIAISDK_CRYPTO_SHA256_SIZE>;
    explicit Crypto(const Sdk &sdk) noexcept : sdk_(sdk.native()) {}

    [[nodiscard]] Sha256Digest sha256(const std::uint8_t *input, std::size_t size) const {
        Sha256Digest output{};
        throwIfError(fiaisdk_crypto_sha256(sdk_, input, size, output.data(), output.size()));
        return output;
    }
    /// Hashes exactly the view's bytes, including embedded NULs.
    [[nodiscard]] Sha256Digest sha256(std::string_view input) const {
        Sha256Digest output{};
        // C++17 has no std::span: access object representations through unsigned char.
        throwIfError(fiaisdk_crypto_sha256(sdk_,
                                           reinterpret_cast<const std::uint8_t *>(input.data()),
                                           input.size(), output.data(), output.size()));
        return output;
    }
    /// Writes directly to caller-owned storage, whose protection/wiping is the caller's duty.
    void randomBytes(std::uint8_t *output, std::size_t size) const {
        throwIfError(fiaisdk_crypto_random_bytes(sdk_, output, size));
    }

    [[nodiscard]] Sha256Digest keccak256(const std::uint8_t *input, std::size_t size) const {
        Sha256Digest output{};
        throwIfError(fiaisdk_crypto_keccak256(sdk_, input, size, output.data(), output.size()));
        return output;
    }
    [[nodiscard]] Sha256Digest keccak256(std::string_view input) const {
        return keccak256(reinterpret_cast<const std::uint8_t *>(input.data()), input.size());
    }
    [[nodiscard]] bool verify(std::int32_t scheme, const std::uint8_t *publicKey,
                              std::size_t publicKeySize, const std::uint8_t *data,
                              std::size_t dataSize, const std::uint8_t *signature,
                              std::size_t signatureSize) const {
        std::uint8_t valid = 0;
        throwIfError(fiaisdk_crypto_verify(sdk_, scheme, publicKey, publicKeySize, data, dataSize,
                                           signature, signatureSize, &valid));
        return valid != 0;
    }

  private:
    const fiaisdk_sdk *sdk_;
};

} // namespace fiaisdk
