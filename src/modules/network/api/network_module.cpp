#include "modules/network/api/network_module.h"

#include <utility>

namespace fiaisdk::network {

NetworkModule::NetworkModule(std::unique_ptr<INodeConnector> connector)
    : connector_(std::move(connector)) {}

core::Result<void> NetworkModule::initialize(const core::ModuleContext &context) {
    if (!connector_) {
        return core::Error::invalidArgument("network module needs a node connector");
    }
    facade_ = std::make_unique<NetworkFacade>(*connector_, context);
    return {};
}

void NetworkModule::shutdown() noexcept {
    if (facade_) {
        try {
            static_cast<void>(facade_->disconnect(NetworkDisconnected::Reason::Shutdown));
        } catch (...) { // NOLINT(bugprone-empty-catch)
            // Only allocation failure is possible here; shutdown must not throw.
        }
        facade_.reset();
    }
}

} // namespace fiaisdk::network
