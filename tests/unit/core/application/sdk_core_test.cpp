#include "core/application/sdk_core.h"
#include "core/infrastructure/in_process_event_bus.h"
#include "support/core/fake_clock.h"
#include "support/core/mock_event_bus.h"
#include "support/core/recording_logger.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fiaisdk::core {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

/// Logger that records its own destruction so the test can check destruction order.
class TrackedLogger final : public ILogger {
  public:
    explicit TrackedLogger(std::vector<std::string> &log) : log_(&log) {}
    ~TrackedLogger() override {
        log_->push_back("logger destroyed");
    }
    TrackedLogger(const TrackedLogger &) = delete;
    TrackedLogger &operator=(const TrackedLogger &) = delete;
    TrackedLogger(TrackedLogger &&) = delete;
    TrackedLogger &operator=(TrackedLogger &&) = delete;

    [[nodiscard]] bool isEnabled(LogLevel /*level*/) const noexcept override {
        return false;
    }
    void write(const LogRecord & /*record*/) noexcept override {}

  private:
    std::vector<std::string> *log_;
};

class LifecycleModule final : public Module {
  public:
    LifecycleModule(std::vector<std::string> &log, bool failInit = false)
        : log_(&log), failInit_(failInit) {}
    ~LifecycleModule() override {
        log_->push_back("module destroyed");
    }
    LifecycleModule(const LifecycleModule &) = delete;
    LifecycleModule &operator=(const LifecycleModule &) = delete;
    LifecycleModule(LifecycleModule &&) = delete;
    LifecycleModule &operator=(LifecycleModule &&) = delete;
    [[nodiscard]] std::string_view name() const noexcept override {
        return "m";
    }
    Result<void> initialize(const ModuleContext &context) override {
        // A module may use shared services during init.
        static_cast<void>(context.clock().now());
        log_->push_back("init");
        if (failInit_) {
            return Error::internal("nope");
        }
        return {};
    }
    void shutdown() noexcept override {
        log_->push_back("shutdown");
    }

  private:
    std::vector<std::string> *log_;
    bool failInit_;
};

CoreServices makeServices(std::vector<std::string> &log) {
    CoreServices services;
    services.clock = std::make_unique<test::FakeClock>();
    services.logger = std::make_unique<TrackedLogger>(log);
    services.eventBus = std::make_unique<InProcessEventBus>(*services.logger);
    return services;
}

TEST(SdkCore, CreateFailsWhenAServiceIsMissing) {
    std::vector<std::string> log;
    CoreServices services = makeServices(log);
    services.eventBus.reset();
    const auto core = SdkCore::create(SdkConfig{}, std::move(services), ModuleRegistry{});
    ASSERT_FALSE(core);
    EXPECT_EQ(core.error().code, ErrorCode::InvalidArgument);
}

TEST(SdkCore, CreateFailsWhenAModuleFailsToInitialize) {
    std::vector<std::string> log;
    {
        ModuleRegistry modules;
        ASSERT_TRUE(modules.add(std::make_unique<LifecycleModule>(log, /*failInit=*/true)));
        const auto core = SdkCore::create(SdkConfig{}, makeServices(log), std::move(modules));
        ASSERT_FALSE(core);
        EXPECT_EQ(core.error().code, ErrorCode::ModuleInitFailed);
    }
    // The failed module is not shut down (its initialize failed) but is still destroyed first.
    EXPECT_THAT(log, ElementsAre("init", "module destroyed", "logger destroyed"));
}

TEST(SdkCore, ExposesConfigModulesAndServices) {
    std::vector<std::string> log;
    ModuleRegistry modules;
    ASSERT_TRUE(modules.add(std::make_unique<LifecycleModule>(log)));
    auto core = SdkCore::create(SdkConfig{"app", "1.0"}, makeServices(log), std::move(modules));
    ASSERT_TRUE(core);
    EXPECT_EQ(core.value()->config().applicationName, "app");
    EXPECT_TRUE(core.value()->hasModule("m"));
    EXPECT_FALSE(core.value()->hasModule("x"));
    EXPECT_FALSE(core.value()->logger().isEnabled(LogLevel::Error));
}

TEST(SdkCore, ModulesShutDownBeforeServicesAreDestroyed) {
    std::vector<std::string> log;
    {
        ModuleRegistry modules;
        ASSERT_TRUE(modules.add(std::make_unique<LifecycleModule>(log)));
        auto core = SdkCore::create(SdkConfig{}, makeServices(log), std::move(modules));
        ASSERT_TRUE(core);
    }
    // Modules are destroyed (not only shut down) before any shared service: guards the member
    // declaration order in SdkCore.
    EXPECT_THAT(log, ElementsAre("init", "shutdown", "module destroyed", "logger destroyed"));
}

/// Module that subscribes to an event during initialize(), as real modules will.
class SubscribingModule final : public Module {
  public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "subscriber";
    }
    Result<void> initialize(const ModuleContext &context) override {
        subscription_ =
            context.eventBus().subscribe("network.connected", [](const DomainEvent &) {});
        return {};
    }
    void shutdown() noexcept override {
        subscription_.cancel();
    }

  private:
    Subscription subscription_;
};

TEST(SdkCore, ModulesReceiveTheEventBusFromTheContext) {
    std::vector<std::string> log;
    auto bus = std::make_unique<test::MockEventBus>();
    EXPECT_CALL(*bus, subscribe("network.connected", _)).WillOnce(Return(Subscription{}));

    CoreServices services;
    services.clock = std::make_unique<test::FakeClock>();
    services.logger = std::make_unique<TrackedLogger>(log);
    services.eventBus = std::move(bus);

    ModuleRegistry modules;
    ASSERT_TRUE(modules.add(std::make_unique<SubscribingModule>()));
    const auto core = SdkCore::create(SdkConfig{}, std::move(services), std::move(modules));
    EXPECT_TRUE(core);
}

} // namespace
} // namespace fiaisdk::core
