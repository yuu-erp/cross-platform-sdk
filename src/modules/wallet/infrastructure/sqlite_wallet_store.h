#pragma once

#include "modules/crypto/api/crypto_facade.h"
#include "modules/wallet/application/ports/i_wallet_store.h"

#include <memory>

namespace fiaisdk::wallet {

/// Opens a dedicated SQLite wallet file. Wallet secrets are sealed with crypto's AES-256-GCM;
/// the store keeps `crypto` alive for its own lifetime, independent of any SDK instance.
[[nodiscard]] core::Result<std::unique_ptr<IWalletStore>>
openSqliteWalletStore(const char *path, std::span<const std::uint8_t, 32> key,
                      std::shared_ptr<const crypto::CryptoFacade> crypto);

} // namespace fiaisdk::wallet
