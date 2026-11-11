#pragma once

#include "modules/network/application/ports/i_node_connector.h"

#include <chrono>
#include <deque>
#include <string>
#include <vector>

namespace fiaisdk::test {

/// Scriptable INodeConnector: returns queued results in order (default: success, 10 ms) and records
/// every probed endpoint. Not thread-safe.
class FakeNodeProbe final : public network::INodeConnector {
  public:
    struct Call {
        std::string url;
        std::chrono::milliseconds timeout;
    };

    void willSucceed(std::chrono::milliseconds latency) {
        script_.emplace_back(network::OpenedConnection{latency});
    }
    void willFail(core::Error error) {
        script_.emplace_back(std::move(error));
    }

    [[nodiscard]] core::Result<network::OpenedConnection>
    open(const network::Endpoint &endpoint, std::chrono::milliseconds timeout) override {
        calls_.push_back({endpoint.url(), timeout});
        if (script_.empty()) {
            return network::OpenedConnection{std::chrono::milliseconds(10)};
        }
        auto next = std::move(script_.front());
        script_.pop_front();
        return next;
    }

    [[nodiscard]] const std::vector<Call> &calls() const {
        return calls_;
    }

  private:
    std::deque<core::Result<network::OpenedConnection>> script_;
    std::vector<Call> calls_;
};

} // namespace fiaisdk::test
