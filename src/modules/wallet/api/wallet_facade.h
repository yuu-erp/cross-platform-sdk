#pragma once
#include "modules/wallet/application/create_wallet.h"
#include "modules/wallet/application/sign_with_wallet.h"
namespace fiaisdk::wallet {
class WalletFacade {
  public:
    explicit WalletFacade(const crypto::CryptoFacade &crypto) noexcept
        : create_(crypto), import_(crypto), sign_(crypto) {}
    [[nodiscard]] core::Result<std::unique_ptr<Wallet>> create() const {
        return create_.execute();
    }
    [[nodiscard]] core::Result<std::unique_ptr<Wallet>>
    import(std::span<const std::uint8_t> bls, std::span<const std::uint8_t> secp) const {
        return import_.execute(bls, secp);
    }
    [[nodiscard]] core::Result<std::vector<std::uint8_t>>
    sign(const Wallet &wallet, crypto::KeyScheme scheme, std::span<const std::uint8_t> data) const {
        return sign_.execute(wallet, scheme, data);
    }

  private:
    CreateWallet create_;
    ImportWallet import_;
    SignWithWallet sign_;
};
} // namespace fiaisdk::wallet
