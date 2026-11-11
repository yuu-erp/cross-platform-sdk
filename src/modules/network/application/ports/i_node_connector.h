#pragma once

#include "core/domain/result.h"
#include "modules/network/domain/endpoint.h"

#include <chrono>
#include <memory>

namespace fiaisdk::network {

/// Owns the resources of one open session. Destruction closes the session.
class NodeConnection {
  public:
    virtual ~NodeConnection() = default;
    /// Check for a known peer closure without waiting or consuming incoming data.
    [[nodiscard]] virtual bool peerClosed() const noexcept {
        return false;
    }
};

struct OpenedConnection {
    std::chrono::milliseconds latency{0};
    std::unique_ptr<NodeConnection> connection{};
};

/// Opens a session within the supplied time budget. Failure leaves any existing session intact.
class INodeConnector {
  public:
    INodeConnector() = default;
    virtual ~INodeConnector() = default;
    INodeConnector(const INodeConnector &) = delete;
    INodeConnector &operator=(const INodeConnector &) = delete;
    INodeConnector(INodeConnector &&) = delete;
    INodeConnector &operator=(INodeConnector &&) = delete;

    [[nodiscard]] virtual core::Result<OpenedConnection>
    open(const Endpoint &endpoint, std::chrono::milliseconds timeout) = 0;
};

} // namespace fiaisdk::network
