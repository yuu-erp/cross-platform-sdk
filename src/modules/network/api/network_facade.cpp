#include "modules/network/api/network_facade.h"

#include <utility>

namespace fiaisdk::network {

NetworkFacade::NetworkFacade(INodeConnector &connector, const core::ModuleContext &context)
    : connectToNode_(connector, context.clock(), context.eventBus(), session_),
      disconnectFromNode_(context.clock(), context.eventBus(), session_),
      log_(context.logger(), "network") {}

core::Result<ConnectionInfo> NetworkFacade::connect(std::string endpointUrl,
                                                    std::chrono::milliseconds timeout) {
    auto result = connectToNode_.execute(ConnectInput{std::move(endpointUrl), timeout});
    try {
        if (result) {
            // origin() only: the path/query may contain API keys.
            log_.info("connected to " + result.value().endpoint.origin());
        } else {
            log_.warning("connect failed: " + result.error().message);
        }
    } catch (...) { // NOLINT(bugprone-empty-catch)
        // Logging is best effort: the connection state is already committed.
    }
    return result;
}

core::Result<void> NetworkFacade::disconnect(NetworkDisconnected::Reason reason) {
    return disconnectFromNode_.execute(reason);
}

std::optional<ConnectionInfo> NetworkFacade::status() const {
    disconnectFromNode_.refreshPeer();
    return session_.current();
}

} // namespace fiaisdk::network
