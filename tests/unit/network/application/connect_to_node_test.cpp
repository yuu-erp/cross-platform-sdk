#include "core/infrastructure/in_process_event_bus.h"
#include "modules/network/application/connect_to_node.h"
#include "modules/network/domain/network_events.h"
#include "support/core/fake_clock.h"
#include "support/core/recording_logger.h"
#include "support/network/fake_node_probe.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace fiaisdk::network {
namespace {

using namespace std::chrono_literals;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

class ConnectToNodeTest : public ::testing::Test {
  protected:
    ConnectToNodeTest() {
        connected_ = core::subscribeTo<NetworkConnected>(bus_, [this](const NetworkConnected &e) {
            events_.push_back("connected " + e.origin());
            sequences_.push_back(e.sequence());
        });
        disconnected_ =
            core::subscribeTo<NetworkDisconnected>(bus_, [this](const NetworkDisconnected &e) {
                events_.push_back("disconnected " + e.origin());
                sequences_.push_back(e.sequence());
            });
    }

    test::FakeClock clock_{core::Timestamp(std::chrono::seconds(1'700'000'000))};
    test::RecordingLogger logger_;
    core::InProcessEventBus bus_{logger_};
    test::FakeNodeProbe probe_;
    NetworkSession session_;
    ConnectToNode connect_{probe_, clock_, bus_, session_};
    std::vector<std::string> events_;
    std::vector<std::uint64_t> sequences_;
    core::Subscription connected_;
    core::Subscription disconnected_;
};

TEST_F(ConnectToNodeTest, ConnectsStoresStateAndPublishesEvent) {
    probe_.willSucceed(42ms);
    const auto result = connect_.execute({"https://node.example.com/rpc", 3s});

    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().latency, 42ms);
    EXPECT_EQ(result.value().connectedAt, clock_.now());
    ASSERT_TRUE(session_.current());
    EXPECT_EQ(session_.current()->endpoint.url(), "https://node.example.com/rpc");
    ASSERT_EQ(probe_.calls().size(), 1U);
    EXPECT_EQ(probe_.calls()[0].timeout, 3s);
    EXPECT_THAT(events_, ElementsAre("connected https://node.example.com"));
}

TEST_F(ConnectToNodeTest, InvalidUrlFailsWithoutProbing) {
    const auto result = connect_.execute({"http://node.example.com", 1s});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::InvalidArgument);
    EXPECT_THAT(probe_.calls(), IsEmpty());
    EXPECT_FALSE(session_.current());
    EXPECT_THAT(events_, IsEmpty());
}

TEST_F(ConnectToNodeTest, TimeoutOutsideRangeIsRejected) {
    EXPECT_FALSE(connect_.execute({"https://node.example.com", 0ms}));
    EXPECT_FALSE(connect_.execute({"https://node.example.com", 60'001ms}));
    EXPECT_TRUE(connect_.execute({"https://node.example.com", 60'000ms}));
}

TEST_F(ConnectToNodeTest, UnreachableNodeReturnsUnavailableAndStaysDisconnected) {
    probe_.willFail(core::Error::unavailable("down"));
    const auto result = connect_.execute({"https://node.example.com", 1s});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::Unavailable);
    EXPECT_FALSE(session_.current());
    EXPECT_THAT(events_, IsEmpty());
}

TEST_F(ConnectToNodeTest, ReconnectingToSameEndpointIsANoOp) {
    ASSERT_TRUE(connect_.execute({"https://node.example.com", 1s}));
    clock_.advance(10s);
    const auto again = connect_.execute({"HTTPS://NODE.example.com:443", 1s});
    ASSERT_TRUE(again);
    EXPECT_EQ(probe_.calls().size(), 1U);               // not probed again
    EXPECT_NE(again.value().connectedAt, clock_.now()); // original connection kept
    EXPECT_EQ(events_.size(), 1U);
}

TEST_F(ConnectToNodeTest, SwitchingEndpointPublishesDisconnectThenConnect) {
    ASSERT_TRUE(connect_.execute({"https://a.example.com", 1s}));
    ASSERT_TRUE(connect_.execute({"https://b.example.com", 1s}));
    EXPECT_EQ(session_.current()->endpoint.host(), "b.example.com");
    EXPECT_THAT(events_,
                ElementsAre("connected https://a.example.com", "disconnected https://a.example.com",
                            "connected https://b.example.com"));
}

TEST_F(ConnectToNodeTest, EventSequencesIncreaseWithEveryChange) {
    ASSERT_TRUE(connect_.execute({"https://a.example.com", 1s}));
    ASSERT_TRUE(connect_.execute({"https://b.example.com", 1s}));
    ASSERT_EQ(sequences_.size(), 3U);
    EXPECT_LT(sequences_[0], sequences_[1]);
    EXPECT_LT(sequences_[1], sequences_[2]);
}

TEST_F(ConnectToNodeTest, WaitForAConcurrentOperationCountsAgainstTheTimeout) {
    const auto busy = session_.lockOperations(); // simulate a connect in progress elsewhere
    const auto start = std::chrono::steady_clock::now();
    const auto result = connect_.execute({"https://node.example.com", 50ms});
    const auto waited = std::chrono::steady_clock::now() - start;

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::Unavailable);
    EXPECT_GE(waited, 40ms);
    EXPECT_LT(waited, 2s); // bounded, not forever
    EXPECT_TRUE(probe_.calls().empty());
}

TEST_F(ConnectToNodeTest, FailedSwitchKeepsThePreviousConnection) {
    ASSERT_TRUE(connect_.execute({"https://a.example.com", 1s}));
    probe_.willFail(core::Error::unavailable("down"));
    EXPECT_FALSE(connect_.execute({"https://b.example.com", 1s}));
    EXPECT_EQ(session_.current()->endpoint.host(), "a.example.com");
    EXPECT_THAT(events_, ElementsAre("connected https://a.example.com"));
}

TEST_F(ConnectToNodeTest, EventsCarryOriginNotSecrets) {
    ASSERT_TRUE(connect_.execute({"https://node.example.com/v1/SECRET_KEY?token=abc", 1s}));
    ASSERT_EQ(events_.size(), 1U);
    EXPECT_EQ(events_[0].find("SECRET"), std::string::npos);
    EXPECT_EQ(events_[0].find("token"), std::string::npos);
}

} // namespace
} // namespace fiaisdk::network
