#include "core/application/module_registry.h"
#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"
#include "support/core/fake_clock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fiaisdk::core {
namespace {

using ::testing::ElementsAre;

/// Records lifecycle calls into a shared log; optionally fails initialize().
class FakeModule final : public Module {
  public:
    FakeModule(std::string name, std::vector<std::string> &log, bool failInit = false)
        : name_(std::move(name)), log_(&log), failInit_(failInit) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return name_;
    }

    Result<void> initialize(const ModuleContext &context) override {
        log_->push_back("init:" + name_);
        receivedClock_ = &context.clock();
        if (failInit_) {
            return Error::internal("boom");
        }
        return {};
    }

    void shutdown() noexcept override {
        log_->push_back("shutdown:" + name_);
    }

    [[nodiscard]] const IClock *receivedClock() const {
        return receivedClock_;
    }

  private:
    std::string name_;
    std::vector<std::string> *log_;
    bool failInit_;
    const IClock *receivedClock_ = nullptr;
};

class ModuleRegistryTest : public ::testing::Test {
  protected:
    test::FakeClock clock_;
    NullLogger logger_;
    InProcessEventBus bus_{logger_};
    ModuleContext context_{clock_, logger_, bus_};
    std::vector<std::string> log_;
};

TEST_F(ModuleRegistryTest, AddRejectsNullEmptyAndDuplicateNames) {
    ModuleRegistry registry;
    EXPECT_FALSE(registry.add(nullptr));
    EXPECT_FALSE(registry.add(std::make_unique<FakeModule>("", log_)));
    EXPECT_TRUE(registry.add(std::make_unique<FakeModule>("a", log_)));
    const auto duplicate = registry.add(std::make_unique<FakeModule>("a", log_));
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(registry.size(), 1U);
}

TEST_F(ModuleRegistryTest, HasMatchesExactName) {
    ModuleRegistry registry;
    ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("wallet", log_)));
    EXPECT_TRUE(registry.has("wallet"));
    EXPECT_FALSE(registry.has("Wallet"));
    EXPECT_FALSE(registry.has("wall"));
    EXPECT_FALSE(registry.has(""));
}

TEST_F(ModuleRegistryTest, InitializesInOrderAndShutsDownInReverse) {
    {
        ModuleRegistry registry;
        ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("a", log_)));
        ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("b", log_)));
        ASSERT_TRUE(registry.initializeAll(context_));
        EXPECT_FALSE(registry.add(std::make_unique<FakeModule>("c", log_)));
        registry.shutdownAll();
        registry.shutdownAll(); // idempotent
    } // destructor must not shut down twice
    EXPECT_THAT(log_, ElementsAre("init:a", "init:b", "shutdown:b", "shutdown:a"));
}

TEST_F(ModuleRegistryTest, ShutdownIsFinal) {
    ModuleRegistry registry;
    ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("a", log_)));
    ASSERT_TRUE(registry.initializeAll(context_));
    registry.shutdownAll();
    EXPECT_FALSE(registry.add(std::make_unique<FakeModule>("b", log_)));
    const auto again = registry.initializeAll(context_);
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code, ErrorCode::InvalidArgument);
    EXPECT_THAT(log_, ElementsAre("init:a", "shutdown:a")); // initialize ran exactly once
}

TEST_F(ModuleRegistryTest, PassesContextToModules) {
    auto module = std::make_unique<FakeModule>("a", log_);
    const FakeModule *raw = module.get();
    ModuleRegistry registry;
    ASSERT_TRUE(registry.add(std::move(module)));
    ASSERT_TRUE(registry.initializeAll(context_));
    EXPECT_EQ(raw->receivedClock(), &clock_);
}

TEST_F(ModuleRegistryTest, InitFailureRollsBackOnlyInitializedModules) {
    ModuleRegistry registry;
    ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("a", log_)));
    ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("b", log_, /*failInit=*/true)));
    ASSERT_TRUE(registry.add(std::make_unique<FakeModule>("c", log_)));
    const auto result = registry.initializeAll(context_);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::ModuleInitFailed);
    EXPECT_EQ(result.error().message, "b: boom");
    EXPECT_THAT(log_, ElementsAre("init:a", "init:b", "shutdown:a"));
}

} // namespace
} // namespace fiaisdk::core
