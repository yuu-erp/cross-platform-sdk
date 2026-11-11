#include "modules/network/application/disconnect_from_node.h"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace fiaisdk::network {

core::Result<void> DisconnectFromNode::execute(NetworkDisconnected::Reason reason) const {
    close(reason, false);
    return {};
}

void DisconnectFromNode::refreshPeer() const {
    close(NetworkDisconnected::Reason::PeerClosed, true);
}

void DisconnectFromNode::close(NetworkDisconnected::Reason reason, bool peerOnly) const {
    std::optional<std::string> closedOrigin;
    std::uint64_t sequence = 0;
    {
        const auto operation = peerOnly
                                   ? session_->lockOperations(std::chrono::milliseconds::zero())
                                   : session_->lockOperations();
        if (!operation.ownsLock() || (peerOnly && !session_->peerClosed())) {
            return;
        }
        const auto current = session_->current();
        if (!current) {
            return;
        }
        closedOrigin = current->endpoint.origin(); // allocate before committing
        sequence = session_->clear();
    }
    events_->publish(
        NetworkDisconnected(clock_->now(), sequence, std::move(*closedOrigin), reason));
}

} // namespace fiaisdk::network
