#pragma once

#include "modules/wallet/application/ports/i_wallet_store.h"

#include <memory>

namespace fiaisdk::wallet {

[[nodiscard]] core::Result<std::unique_ptr<IWalletStore>>
openSqliteWalletStore(const char *path, std::span<const std::uint8_t, 32> key);

} // namespace fiaisdk::wallet
