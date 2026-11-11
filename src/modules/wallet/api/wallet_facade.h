#pragma once
#include "modules/wallet/application/create_wallet.h"
#include "modules/wallet/application/ports/i_wallet_store.h"
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

    [[nodiscard]] core::Result<std::unique_ptr<Wallet>> load(IWalletStore &store,
                                                             const WalletAddress &address) const {
        auto keys = store.load(address);
        if (!keys) {
            return keys.error();
        }
        const auto bytes = keys.value().view();
        if (bytes.size() != 64) {
            return core::Error::internal("invalid stored wallet key size");
        }
        auto wallet = import(bytes.first<32>(), bytes.last<32>());
        if (!wallet) {
            return wallet.error();
        }
        if (wallet.value()->info().address != address) {
            return core::Error::internal("stored wallet address mismatch");
        }
        return wallet;
    }

  private:
    CreateWallet create_;
    ImportWallet import_;
    SignWithWallet sign_;
};
} // namespace fiaisdk::wallet
