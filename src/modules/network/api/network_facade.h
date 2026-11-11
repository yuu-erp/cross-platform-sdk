#pragma once

#include "core/application/logger.h"
#include "core/application/module_context.h"
#include "core/domain/result.h"
#include "modules/network/application/connect_to_node.h"
#include "modules/network/application/disconnect_from_node.h"
#include "modules/network/application/network_session.h"
#include "modules/network/application/ports/i_node_connector.h"
#include "modules/network/domain/connection_info.h"

#include <chrono>
#include <optional>
#include <string>

namespace fiaisdk::network {

/// Entry point of the network module for bindings and other modules. Thread-safe.
/// Owns the session and the use cases; does not own the connector or the core services.
class NetworkFacade {
  public:
    NetworkFacade(INodeConnector &connector, const core::ModuleContext &context);

    /// See ConnectToNode. Blocks up to `timeout`.
    [[nodiscard]] core::Result<ConnectionInfo> connect(std::string endpointUrl,
                                                       std::chrono::milliseconds timeout);
    [[nodiscard]] core::Result<void>
    disconnect(NetworkDisconnected::Reason reason = NetworkDisconnected::Reason::Requested);
    /// Current connection, or nullopt when disconnected. Never blocks on a connect in progress.
    [[nodiscard]] std::optional<ConnectionInfo> status() const;

  private:
    NetworkSession session_;
    ConnectToNode connectToNode_;
    DisconnectFromNode disconnectFromNode_;
    core::Logger log_;
};

} // namespace fiaisdk::network
