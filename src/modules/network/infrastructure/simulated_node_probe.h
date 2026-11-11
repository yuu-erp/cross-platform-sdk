#pragma once

#include "modules/network/application/ports/i_node_connector.h"

namespace fiaisdk::network {

/// DEMO adapter for INodeConnector — performs NO network I/O.
///
/// Built into internal tests only. Deterministic behavior:
/// - hosts ending in ".invalid" (RFC 2606 reserved TLD) are unreachable → Unavailable;
/// - every other host "answers" with a latency of 5..49 ms derived from the host name;
/// - if that latency exceeds `timeout` → Unavailable (simulated timeout).
/// Thread-safe (stateless).
class SimulatedNodeProbe final : public INodeConnector {
  public:
    [[nodiscard]] core::Result<OpenedConnection> open(const Endpoint &endpoint,
                                                      std::chrono::milliseconds timeout) override;

    /// The latency this adapter reports for `host` (exposed for tests and demos).
    [[nodiscard]] static std::chrono::milliseconds
    simulatedLatency(const std::string &host) noexcept;
};

} // namespace fiaisdk::network
