#pragma once
#include "core/application/module.h"
#include "modules/wallet/api/wallet_facade.h"
#include <memory>
namespace fiaisdk::wallet {
class WalletModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "wallet";
    explicit WalletModule(const crypto::CryptoFacade &crypto) noexcept : crypto_(&crypto) {}
    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] core::Result<void> initialize(const core::ModuleContext &context) override;
    void shutdown() noexcept override;
    [[nodiscard]] WalletFacade *facade() const noexcept {
        return facade_.get();
    }

  private:
    const crypto::CryptoFacade *crypto_;
    std::unique_ptr<WalletFacade> facade_;
};
} // namespace fiaisdk::wallet
