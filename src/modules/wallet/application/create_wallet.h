#pragma once
#include "modules/wallet/application/import_wallet.h"
namespace fiaisdk::wallet {
class CreateWallet {
  public:
    explicit CreateWallet(const crypto::CryptoFacade &crypto) noexcept
        : crypto_(&crypto), import_(crypto) {}
    [[nodiscard]] core::Result<std::unique_ptr<Wallet>> execute() const {
        auto bls = crypto_->generatePrivateKey(crypto::KeyScheme::Bls12381);
        if (!bls) {
            return bls.error();
        }
        auto secp = crypto_->generatePrivateKey(crypto::KeyScheme::Secp256k1);
        if (!secp) {
            return secp.error();
        }
        return import_.execute(bls.value().view(), secp.value().view());
    }

  private:
    const crypto::CryptoFacade *crypto_;
    ImportWallet import_;
};
} // namespace fiaisdk::wallet
