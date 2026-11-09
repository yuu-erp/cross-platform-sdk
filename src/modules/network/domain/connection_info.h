#pragma once

#include "core/domain/timestamp.h"
#include "modules/network/domain/endpoint.h"

#include <chrono>

namespace fiaisdk::network {

/// Facts about the current connection. Value type.
struct ConnectionInfo {
    Endpoint endpoint;
    /// Round-trip time measured when the connection was established.
    std::chrono::milliseconds latency{0};
    core::Timestamp connectedAt;

    friend bool operator==(const ConnectionInfo &, const ConnectionInfo &) = default;
};

} // namespace fiaisdk::network
