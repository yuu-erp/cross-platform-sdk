#pragma once

#include "core/application/module.h"
#include "modules/crypto/api/crypto_facade.h"

#include <memory>

namespace fiaisdk::crypto {

class CryptoModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "crypto";
    explicit CryptoModule(std::unique_ptr<ICryptoProvider> provider);
    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] core::Result<void> initialize(const core::ModuleContext &context) override;
    void shutdown() noexcept override;
    [[nodiscard]] CryptoFacade *facade() const noexcept {
        return facade_.get();
    }

  private:
    std::unique_ptr<ICryptoProvider> provider_;
    std::unique_ptr<CryptoFacade> facade_;
};

} // namespace fiaisdk::crypto
