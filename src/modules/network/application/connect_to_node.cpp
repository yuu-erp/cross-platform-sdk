#include "modules/network/application/connect_to_node.h"

#include "modules/network/domain/network_events.h"

#include <optional>
#include <string>
#include <utility>

namespace fiaisdk::network {

namespace {

std::chrono::milliseconds elapsedSince(core::MonotonicTime start, const core::IClock &clock) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock.monotonicNow() - start);
}

} // namespace

core::Result<ConnectionInfo> ConnectToNode::execute(const ConnectInput &input) const {
    auto endpoint = Endpoint::parse(input.endpointUrl);
    if (!endpoint) {
        return endpoint.error();
    }
    if (input.timeout < kMinTimeout || input.timeout > kMaxTimeout) {
        return core::Error::invalidArgument("timeout must be between 1 ms and 60000 ms");
    }

    const core::MonotonicTime start = clock_->monotonicNow();
    ConnectionInfo connected{std::move(endpoint).value(), {}, {}};
    std::optional<std::string> previousOrigin;
    std::string connectedOrigin;
    std::uint64_t disconnectSequence = 0;
    std::uint64_t connectSequence = 0;
    {
        auto operation = session_->lockOperations(input.timeout);
        if (!operation.ownsLock()) {
            return core::Error::unavailable("another connect/disconnect did not finish within " +
                                            std::to_string(input.timeout.count()) + " ms");
        }

        const auto previous = session_->current();
        if (previous && previous->endpoint == connected.endpoint) {
            return *previous; // idempotent: nothing changes, nothing published
        }

        const auto remaining = input.timeout - elapsedSince(start, *clock_);
        if (remaining < kMinTimeout) {
            return core::Error::unavailable("timed out waiting for a concurrent operation");
        }
        auto probed = probe_->probe(connected.endpoint, remaining);
        if (!probed) {
            return probed.error(); // previous connection, if any, stays in place
        }

        // Allocate everything before committing, so a bad_alloc cannot leave the state changed
        // without the matching events.
        connected.latency = probed.value().latency;
        connected.connectedAt = clock_->now();
        connectedOrigin = connected.endpoint.origin();
        if (previous) {
            previousOrigin = previous->endpoint.origin();
        }
        ConnectionInfo stored = connected;

        // Commit: nothing below throws.
        if (previousOrigin) {
            const auto sequences = session_->replace(std::move(stored));
            disconnectSequence = sequences.disconnectSequence;
            connectSequence = sequences.connectSequence;
        } else {
            connectSequence = session_->set(std::move(stored));
        }
    }

    // Publish outside the locks: handlers may call back into the network module.
    if (previousOrigin) {
        events_->publish(NetworkDisconnected(clock_->now(), disconnectSequence,
                                             std::move(*previousOrigin),
                                             NetworkDisconnected::Reason::SwitchingEndpoint));
    }
    events_->publish(NetworkConnected(connected.connectedAt, connectSequence,
                                      std::move(connectedOrigin), connected.latency));
    return connected;
}

} // namespace fiaisdk::network
