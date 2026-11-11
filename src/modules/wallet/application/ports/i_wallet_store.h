#pragma once

#include "core/domain/result.h"
#include "modules/wallet/domain/wallet.h"

#include <vector>

namespace fiaisdk::wallet {

class IWalletStore {
  public:
    virtual ~IWalletStore() = default;
    [[nodiscard]] virtual core::Result<void> save(const Wallet &wallet) = 0;
    [[nodiscard]] virtual core::Result<core::SecureBytes> load(const WalletAddress &address) = 0;
    [[nodiscard]] virtual core::Result<std::vector<WalletAddress>> addresses() = 0;
    [[nodiscard]] virtual core::Result<void> remove(const WalletAddress &address) = 0;
};

} // namespace fiaisdk::wallet
