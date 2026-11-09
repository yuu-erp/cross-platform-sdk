#pragma once

#include "core/domain/result.h"
#include "modules/network/domain/endpoint.h"

#include <chrono>

namespace fiaisdk::network {

struct ProbeResult {
    std::chrono::milliseconds latency{0};
};

/// Port: checks that a node at `endpoint` answers, within `timeout`.
///
/// Implementations (infrastructure/) do the real I/O. Contract:
/// - return ProbeResult on success;
/// - return ErrorCode::Unavailable when the node cannot be reached or does not answer in time;
/// - must not throw; must return within roughly `timeout`; must be thread-safe.
class INodeProbe {
  public:
    INodeProbe() = default;
    virtual ~INodeProbe() = default;
    INodeProbe(const INodeProbe &) = delete;
    INodeProbe &operator=(const INodeProbe &) = delete;
    INodeProbe(INodeProbe &&) = delete;
    INodeProbe &operator=(INodeProbe &&) = delete;

    [[nodiscard]] virtual core::Result<ProbeResult> probe(const Endpoint &endpoint,
                                                          std::chrono::milliseconds timeout) = 0;
};

} // namespace fiaisdk::network
