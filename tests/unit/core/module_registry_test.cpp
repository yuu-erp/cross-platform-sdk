#include "core/module_registry.h"
#include "core/sdk_core.h"

#include "test_check.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using fiaisdk::core::Module;
using fiaisdk::core::ModuleRegistry;
using fiaisdk::core::SdkConfig;
using fiaisdk::core::SdkCore;
using fiaisdk::kernel::Error;
using fiaisdk::kernel::ErrorCode;
using fiaisdk::kernel::Result;

namespace {

/// Records lifecycle calls into a shared log; optionally fails initialize().
class FakeModule final : public Module {
  public:
    FakeModule(std::string name, std::vector<std::string> &log, bool failInit = false)
        : name_(std::move(name)), log_(log), failInit_(failInit) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return name_;
    }

    Result<void> initialize() override {
        log_.push_back("init:" + name_);
        if (failInit_) {
            return Error{ErrorCode::Internal, "boom"};
        }
        return {};
    }

    void shutdown() noexcept override {
        log_.push_back("shutdown:" + name_);
    }

  private:
    std::string name_;
    std::vector<std::string> &log_;
    bool failInit_;
};

void add_rejectsNullEmptyAndDuplicate() {
    std::vector<std::string> log;
    ModuleRegistry registry;
    CHECK(!registry.add(nullptr));
    CHECK(!registry.add(std::make_unique<FakeModule>("", log)));
    CHECK(registry.add(std::make_unique<FakeModule>("a", log)).hasValue());
    auto dup = registry.add(std::make_unique<FakeModule>("a", log));
    CHECK(!dup);
    CHECK(dup.error().code == ErrorCode::InvalidArgument);
    CHECK(registry.size() == 1);
}

void has_matchesExactName() {
    std::vector<std::string> log;
    ModuleRegistry registry;
    CHECK(registry.add(std::make_unique<FakeModule>("wallet", log)).hasValue());
    CHECK(registry.has("wallet"));
    CHECK(!registry.has("Wallet"));
    CHECK(!registry.has("wall"));
    CHECK(!registry.has(""));
}

void initializeAll_runsInOrder_shutdownRunsInReverse() {
    std::vector<std::string> log;
    {
        ModuleRegistry registry;
        CHECK(registry.add(std::make_unique<FakeModule>("a", log)).hasValue());
        CHECK(registry.add(std::make_unique<FakeModule>("b", log)).hasValue());
        CHECK(registry.initializeAll().hasValue());
        CHECK(!registry.add(std::make_unique<FakeModule>("c", log)));
        registry.shutdownAll();
        registry.shutdownAll(); // idempotent
    } // destructor must not shut down twice
    const std::vector<std::string> expected{"init:a", "init:b", "shutdown:b", "shutdown:a"};
    CHECK(log == expected);
}

void initializeAll_failure_rollsBackInitializedModulesOnly() {
    std::vector<std::string> log;
    ModuleRegistry registry;
    CHECK(registry.add(std::make_unique<FakeModule>("a", log)).hasValue());
    CHECK(registry.add(std::make_unique<FakeModule>("b", log, /*failInit=*/true)).hasValue());
    CHECK(registry.add(std::make_unique<FakeModule>("c", log)).hasValue());
    auto result = registry.initializeAll();
    CHECK(!result);
    CHECK(result.error().code == ErrorCode::ModuleInitFailed);
    const std::vector<std::string> expected{"init:a", "init:b", "shutdown:a"};
    CHECK(log == expected);
}

void sdkCore_create_failsWhenModuleFails_andShutsDownOnDestroy() {
    std::vector<std::string> log;
    {
        ModuleRegistry failing;
        CHECK(failing.add(std::make_unique<FakeModule>("x", log, true)).hasValue());
        auto core = SdkCore::create(SdkConfig{}, std::move(failing));
        CHECK(!core);
    }
    log.clear();
    {
        ModuleRegistry ok;
        CHECK(ok.add(std::make_unique<FakeModule>("y", log)).hasValue());
        auto core = SdkCore::create(SdkConfig{"app", "1.0"}, std::move(ok));
        CHECK(core.hasValue());
        CHECK(core.value()->hasModule("y"));
        CHECK(core.value()->config().applicationName == "app");
    }
    const std::vector<std::string> expected{"init:y", "shutdown:y"};
    CHECK(log == expected);
}

} // namespace

int main() {
    add_rejectsNullEmptyAndDuplicate();
    has_matchesExactName();
    initializeAll_runsInOrder_shutdownRunsInReverse();
    initializeAll_failure_rollsBackInitializedModulesOnly();
    sdkCore_create_failsWhenModuleFails_andShutsDownOnDestroy();
    return TEST_RESULT();
}
