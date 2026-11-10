// SPDX-License-Identifier: UNLICENSED
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <fiaisdk/cpp/sdk.hpp>
#include <fiaisdk/wallet.h>
#include <string>
#include <string_view>
#include <utility>

namespace fiaisdk {

/// Move-only offline wallet. Keys are never exposed; creating/signing borrows an SDK.
/// The SDK used at construction must remain alive for signing, but not for info/destruction.
class Wallet {
  public:
    using BlsSignature = std::array<std::uint8_t, 96>;
    using Secp256k1Signature = std::array<std::uint8_t, 65>;
    explicit Wallet(const Sdk &sdk) : sdk_(sdk.native()) {
        throwIfError(fiaisdk_wallet_create(sdk_, &handle_));
    }
    [[nodiscard]] static Wallet import(const Sdk &sdk, const std::uint8_t *bls, std::size_t blsSize,
                                       const std::uint8_t *secp, std::size_t secpSize) {
        fiaisdk_wallet *handle = nullptr;
        throwIfError(fiaisdk_wallet_import(sdk.native(), bls, blsSize, secp, secpSize, &handle));
        return Wallet(sdk.native(), handle);
    }
    ~Wallet() {
        fiaisdk_wallet_destroy(handle_);
    }
    Wallet(const Wallet &) = delete;
    Wallet &operator=(const Wallet &) = delete;
    Wallet(Wallet &&other) noexcept
        : sdk_(std::exchange(other.sdk_, nullptr)), handle_(std::exchange(other.handle_, nullptr)) {
    }
    Wallet &operator=(Wallet &&other) noexcept {
        if (this != &other) {
            fiaisdk_wallet_destroy(handle_);
            sdk_ = std::exchange(other.sdk_, nullptr);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    [[nodiscard]] fiaisdk_wallet_info info() const {
        fiaisdk_wallet_info output = FIAISDK_WALLET_INFO_INIT;
        throwIfError(fiaisdk_wallet_get_info(handle_, &output));
        return output;
    }
    /// Lower-case 40-character hex, without 0x, matching the observed legacy representation.
    [[nodiscard]] std::string address() const {
        const auto data = info();
        return hex(data.address);
    }
    [[nodiscard]] std::string blsAddress() const {
        const auto data = info();
        return hex(data.bls_address);
    }
    [[nodiscard]] BlsSignature signBls(const std::uint8_t *data, std::size_t size) const {
        BlsSignature output{};
        throwIfError(fiaisdk_wallet_sign(sdk_, handle_, FIAISDK_CRYPTO_SCHEME_BLS12381, data, size,
                                         output.data(), output.size()));
        return output;
    }
    [[nodiscard]] BlsSignature signBls(std::string_view data) const {
        return signBls(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
    }
    [[nodiscard]] Secp256k1Signature signDigest(const std::array<std::uint8_t, 32> &digest) const {
        Secp256k1Signature output{};
        throwIfError(fiaisdk_wallet_sign(sdk_, handle_, FIAISDK_CRYPTO_SCHEME_SECP256K1,
                                         digest.data(), digest.size(), output.data(),
                                         output.size()));
        return output;
    }
    [[nodiscard]] const fiaisdk_wallet *native() const noexcept {
        return handle_;
    }

  private:
    Wallet(const fiaisdk_sdk *sdk, fiaisdk_wallet *handle) noexcept : sdk_(sdk), handle_(handle) {}
    static std::string hex(const std::uint8_t (&bytes)[20]) {
        constexpr char kDigits[] = "0123456789abcdef";
        std::string output;
        output.reserve(40);
        for (auto byte : bytes) {
            output.push_back(kDigits[byte >> 4]);
            output.push_back(kDigits[byte & 15]);
        }
        return output;
    }
    const fiaisdk_sdk *sdk_ = nullptr;
    fiaisdk_wallet *handle_ = nullptr;
};
} // namespace fiaisdk
