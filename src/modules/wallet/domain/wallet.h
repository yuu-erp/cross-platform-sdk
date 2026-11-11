#pragma once

#include "core/domain/secure_bytes.h"

#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace fiaisdk::wallet {

using WalletAddress = std::array<std::uint8_t, 20>;

struct WalletInfo {
    std::array<std::uint8_t, 48> blsPublicKey{};
    std::array<std::uint8_t, 65> secpPublicKey{};
    WalletAddress address{};
    WalletAddress blsAddress{};
};

/// In-memory dual-key wallet. Created only after application/provider validation.
class Wallet {
  public:
    Wallet(core::SecureBytes bls, core::SecureBytes secp, WalletInfo info)
        : bls_(std::move(bls)), secp_(std::move(secp)), info_(info) {}
    Wallet(const Wallet &) = delete;
    Wallet &operator=(const Wallet &) = delete;
    [[nodiscard]] const WalletInfo &info() const noexcept {
        return info_;
    }
    [[nodiscard]] std::span<const std::uint8_t> blsSecret() const noexcept {
        return bls_.view();
    }
    [[nodiscard]] std::span<const std::uint8_t> secpSecret() const noexcept {
        return secp_.view();
    }

  private:
    core::SecureBytes bls_;
    core::SecureBytes secp_;
    WalletInfo info_;
};

} // namespace fiaisdk::wallet
