#pragma once

#include "core/application/module.h"
#include "modules/network/api/network_facade.h"
#include "modules/network/application/ports/i_node_connector.h"

#include <memory>
#include <string_view>

namespace fiaisdk::network {

/// Lifecycle of the network module. The connector adapter is injected by the composition root.
class NetworkModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "network";

    explicit NetworkModule(std::unique_ptr<INodeConnector> connector);

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] core::Result<void> initialize(const core::ModuleContext &context) override;
    /// Disconnects (publishing NetworkDisconnected{Shutdown}) and releases the facade.
    void shutdown() noexcept override;

    /// Valid between initialize() and shutdown(); nullptr otherwise.
    [[nodiscard]] NetworkFacade *facade() noexcept {
        return facade_.get();
    }

  private:
    std::unique_ptr<INodeConnector> connector_;
    std::unique_ptr<NetworkFacade> facade_;
};

} // namespace fiaisdk::network
