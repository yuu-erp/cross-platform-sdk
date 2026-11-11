#pragma once

#include "core/domain/result.h"
#include "modules/wallet/domain/wallet.h"

#include <vector>

namespace fiaisdk::wallet {

/// Persistence port for wallet secrets. Implementations are thread-safe.
class IWalletStore {
  public:
    IWalletStore() = default;
    IWalletStore(const IWalletStore &) = delete;
    IWalletStore &operator=(const IWalletStore &) = delete;
    IWalletStore(IWalletStore &&) = delete;
    IWalletStore &operator=(IWalletStore &&) = delete;
    virtual ~IWalletStore() = default;
    [[nodiscard]] virtual core::Result<void> save(const Wallet &wallet) = 0;
    [[nodiscard]] virtual core::Result<core::SecureBytes> load(const WalletAddress &address) = 0;
    [[nodiscard]] virtual core::Result<std::vector<WalletAddress>> addresses() = 0;
    [[nodiscard]] virtual core::Result<void> remove(const WalletAddress &address) = 0;
};

} // namespace fiaisdk::wallet
