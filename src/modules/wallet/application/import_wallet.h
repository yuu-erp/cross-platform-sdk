#pragma once
#include "modules/crypto/api/crypto_facade.h"
#include "modules/wallet/domain/wallet.h"
#include <memory>
namespace fiaisdk::wallet {
class ImportWallet {
  public:
    explicit ImportWallet(const crypto::CryptoFacade &crypto) noexcept : crypto_(&crypto) {}
    [[nodiscard]] core::Result<std::unique_ptr<Wallet>>
    execute(std::span<const std::uint8_t> bls, std::span<const std::uint8_t> secp) const;

  private:
    const crypto::CryptoFacade *crypto_;
};
} // namespace fiaisdk::wallet
