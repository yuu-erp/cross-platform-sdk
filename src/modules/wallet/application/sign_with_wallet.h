#pragma once
#include "modules/crypto/api/crypto_facade.h"
#include "modules/wallet/domain/wallet.h"
namespace fiaisdk::wallet {
class SignWithWallet {
  public:
    explicit SignWithWallet(const crypto::CryptoFacade &crypto) noexcept : crypto_(&crypto) {}
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    execute(const Wallet &wallet, crypto::KeyScheme scheme,
            std::span<const std::uint8_t> data) const {
        if (scheme == crypto::KeyScheme::Bls12381) {
            return crypto_->sign(scheme, wallet.blsSecret(), data);
        }
        if (scheme == crypto::KeyScheme::Secp256k1) {
            return crypto_->sign(scheme, wallet.secpSecret(), data);
        }
        return core::Error::invalidArgument("unsupported signing scheme");
    }

  private:
    const crypto::CryptoFacade *crypto_;
};
} // namespace fiaisdk::wallet
