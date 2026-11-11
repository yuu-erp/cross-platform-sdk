#pragma once

#include "modules/network/application/ports/i_node_connector.h"

namespace fiaisdk::network {

class TcpNodeConnector final : public INodeConnector {
  public:
    [[nodiscard]] core::Result<OpenedConnection> open(const Endpoint &endpoint,
                                                      std::chrono::milliseconds timeout) override;
};

} // namespace fiaisdk::network
