#pragma once

#include "core/application/module.h"

#include <string_view>

namespace fiaisdk::wallet {

/// Lifecycle entry point of the wallet module. Placeholder: no use cases yet.
class WalletModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "wallet";

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
};

} // namespace fiaisdk::wallet
