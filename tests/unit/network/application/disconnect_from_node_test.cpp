#include "core/infrastructure/in_process_event_bus.h"
#include "modules/network/application/connect_to_node.h"
#include "modules/network/application/disconnect_from_node.h"
#include "support/core/fake_clock.h"
#include "support/core/recording_logger.h"
#include "support/network/fake_node_probe.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>

namespace fiaisdk::network {
namespace {

using namespace std::chrono_literals;

class DisconnectFromNodeTest : public ::testing::Test {
  protected:
    DisconnectFromNodeTest() {
        subscription_ =
            core::subscribeTo<NetworkDisconnected>(bus_, [this](const NetworkDisconnected &e) {
                lastReason_ = e.reason();
                ++count_;
            });
    }

    test::FakeClock clock_;
    test::RecordingLogger logger_;
    core::InProcessEventBus bus_{logger_};
    test::FakeNodeProbe connector_;
    NetworkSession session_;
    ConnectToNode connect_{connector_, clock_, bus_, session_};
    DisconnectFromNode disconnect_{clock_, bus_, session_};
    core::Subscription subscription_;
    std::optional<NetworkDisconnected::Reason> lastReason_;
    int count_ = 0;
};

TEST_F(DisconnectFromNodeTest, NotConnectedIsANoOp) {
    EXPECT_TRUE(disconnect_.execute());
    EXPECT_EQ(count_, 0);
}

TEST_F(DisconnectFromNodeTest, ClearsSessionAndPublishesReason) {
    ASSERT_TRUE(connect_.execute({"https://node.example.com", 1s}));
    EXPECT_TRUE(disconnect_.execute(NetworkDisconnected::Reason::Shutdown));
    EXPECT_FALSE(session_.current());
    EXPECT_EQ(count_, 1);
    EXPECT_EQ(lastReason_, NetworkDisconnected::Reason::Shutdown);

    EXPECT_TRUE(disconnect_.execute()); // second time: nothing to do
    EXPECT_EQ(count_, 1);
}

} // namespace
} // namespace fiaisdk::network
