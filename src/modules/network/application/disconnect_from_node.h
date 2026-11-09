#pragma once

#include "core/application/ports/i_clock.h"
#include "core/application/ports/i_event_bus.h"
#include "core/domain/result.h"
#include "modules/network/application/network_session.h"
#include "modules/network/domain/network_events.h"

namespace fiaisdk::network {

/// Use case: close the current connection.
/// Not connected → success, nothing published. Otherwise clears the session and publishes
/// NetworkDisconnected(reason) after the lock is released.
/// Blocking: waits for a connect in progress (bounded by that connect's timeout).
class DisconnectFromNode {
  public:
    DisconnectFromNode(core::IClock &clock, core::IEventBus &events,
                       NetworkSession &session) noexcept
        : clock_(&clock), events_(&events), session_(&session) {}

    [[nodiscard]] core::Result<void>
    execute(NetworkDisconnected::Reason reason = NetworkDisconnected::Reason::Requested) const;

  private:
    core::IClock *clock_;
    core::IEventBus *events_;
    NetworkSession *session_;
};

} // namespace fiaisdk::network
