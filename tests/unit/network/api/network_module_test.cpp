#include "core/infrastructure/in_process_event_bus.h"
#include "modules/network/api/network_module.h"
#include "support/core/fake_clock.h"
#include "support/core/recording_logger.h"
#include "support/network/fake_node_probe.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>

namespace fiaisdk::network {
namespace {

using namespace std::chrono_literals;

class NetworkModuleTest : public ::testing::Test {
  protected:
    test::FakeClock clock_;
    test::RecordingLogger logger_;
    core::InProcessEventBus bus_{logger_};
    core::ModuleContext context_{clock_, logger_, bus_};
};

TEST_F(NetworkModuleTest, FacadeExistsOnlyBetweenInitializeAndShutdown) {
    NetworkModule module(std::make_unique<test::FakeNodeProbe>());
    EXPECT_EQ(module.name(), "network");
    EXPECT_EQ(module.facade(), nullptr);
    ASSERT_TRUE(module.initialize(context_));
    ASSERT_NE(module.facade(), nullptr);
    module.shutdown();
    EXPECT_EQ(module.facade(), nullptr);
}

TEST_F(NetworkModuleTest, InitializeWithoutProbeFails) {
    NetworkModule module(nullptr);
    const auto result = module.initialize(context_);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::InvalidArgument);
}

TEST_F(NetworkModuleTest, ShutdownDisconnectsWithShutdownReason) {
    std::optional<NetworkDisconnected::Reason> reason;
    auto subscription = core::subscribeTo<NetworkDisconnected>(
        bus_, [&](const NetworkDisconnected &e) { reason = e.reason(); });

    NetworkModule module(std::make_unique<test::FakeNodeProbe>());
    ASSERT_TRUE(module.initialize(context_));
    ASSERT_TRUE(module.facade()->connect("https://node.example.com", 1s));
    module.shutdown();
    EXPECT_EQ(reason, NetworkDisconnected::Reason::Shutdown);
}

TEST_F(NetworkModuleTest, FacadeLogsOriginButNeverTheSecretPath) {
    NetworkModule module(std::make_unique<test::FakeNodeProbe>());
    ASSERT_TRUE(module.initialize(context_));
    ASSERT_TRUE(module.facade()->connect("https://node.example.com/KEY123?t=SECRET", 1s));
    for (const auto &entry : logger_.entries()) {
        EXPECT_EQ(entry.message.find("KEY123"), std::string::npos) << entry.message;
        EXPECT_EQ(entry.message.find("SECRET"), std::string::npos) << entry.message;
    }
    module.shutdown();
}

} // namespace
} // namespace fiaisdk::network
