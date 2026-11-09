#include "modules/network/application/disconnect_from_node.h"

#include <optional>
#include <string>
#include <utility>

namespace fiaisdk::network {

core::Result<void> DisconnectFromNode::execute(NetworkDisconnected::Reason reason) const {
    std::optional<std::string> closedOrigin;
    std::uint64_t sequence = 0;
    {
        const auto operation = session_->lockOperations(); // waits for a connect in flight
        const auto current = session_->current();
        if (!current) {
            return {};
        }
        closedOrigin = current->endpoint.origin(); // allocate before committing
        sequence = session_->clear();
    }
    events_->publish(
        NetworkDisconnected(clock_->now(), sequence, std::move(*closedOrigin), reason));
    return {};
}

} // namespace fiaisdk::network
