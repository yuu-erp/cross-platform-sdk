#pragma once

#include "modules/network/application/ports/i_node_probe.h"

namespace fiaisdk::network {

/// DEMO adapter for INodeProbe — performs NO network I/O.
///
/// Stands in for the real HTTP/WebSocket probe until the transport ADR (HTTP/WebSocket client
/// library) is decided. Deterministic, so demos and tests behave the same everywhere:
/// - hosts ending in ".invalid" (RFC 2606 reserved TLD) are unreachable → Unavailable;
/// - every other host "answers" with a latency of 5..49 ms derived from the host name;
/// - if that latency exceeds `timeout` → Unavailable (simulated timeout).
/// Thread-safe (stateless).
class SimulatedNodeProbe final : public INodeProbe {
  public:
    [[nodiscard]] core::Result<ProbeResult> probe(const Endpoint &endpoint,
                                                  std::chrono::milliseconds timeout) override;

    /// The latency this adapter reports for `host` (exposed for tests and demos).
    [[nodiscard]] static std::chrono::milliseconds
    simulatedLatency(const std::string &host) noexcept;
};

} // namespace fiaisdk::network
