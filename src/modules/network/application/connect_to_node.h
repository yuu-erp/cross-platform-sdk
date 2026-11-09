#pragma once

#include "core/application/ports/i_clock.h"
#include "core/application/ports/i_event_bus.h"
#include "core/domain/result.h"
#include "modules/network/application/network_session.h"
#include "modules/network/application/ports/i_node_probe.h"
#include "modules/network/domain/connection_info.h"

#include <chrono>
#include <string>

namespace fiaisdk::network {

struct ConnectInput {
    std::string endpointUrl;
    std::chrono::milliseconds timeout{std::chrono::seconds(5)};
};

/// Use case: connect the SDK to a node.
///
/// 1. validate the URL (Endpoint::parse) and the timeout (1 ms .. 60 s) → InvalidArgument;
/// 2. wait for a concurrent connect/disconnect to finish; the wait counts against `timeout`
///    → Unavailable if the timeout runs out;
/// 3. already connected to the same endpoint → return it unchanged (no probe, no event);
/// 4. probe the node with the remaining time → Unavailable on failure, and the previous
///    connection (if any) is KEPT (switching is all-or-nothing, also when memory runs out:
///    everything that allocates happens before the state is committed);
/// 5. store the connection; publish NetworkDisconnected(SwitchingEndpoint) for the previous one,
///    then NetworkConnected, after all locks are released.
///
/// Blocking: at most about `timeout` in total on the calling thread.
class ConnectToNode {
  public:
    static constexpr std::chrono::milliseconds kMinTimeout{1};
    static constexpr std::chrono::milliseconds kMaxTimeout{60'000};

    ConnectToNode(INodeProbe &probe, core::IClock &clock, core::IEventBus &events,
                  NetworkSession &session) noexcept
        : probe_(&probe), clock_(&clock), events_(&events), session_(&session) {}

    [[nodiscard]] core::Result<ConnectionInfo> execute(const ConnectInput &input) const;

  private:
    INodeProbe *probe_;
    core::IClock *clock_;
    core::IEventBus *events_;
    NetworkSession *session_;
};

} // namespace fiaisdk::network
