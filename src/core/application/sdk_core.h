#pragma once

#include "core/application/module_context.h"
#include "core/application/module_registry.h"
#include "core/application/ports/i_clock.h"
#include "core/application/ports/i_event_bus.h"
#include "core/application/ports/i_logger.h"
#include "core/domain/result.h"

#include <memory>
#include <string>
#include <string_view>

namespace fiaisdk::core {

/// Internal, C++-typed copy of fiaisdk_sdk_config.
struct SdkConfig {
    std::string applicationName;
    std::string applicationVersion;
};

/// Concrete implementations of the shared ports, chosen by the composition root.
/// Destruction order is eventBus → logger → clock (reverse of declaration), so the bus may log.
struct CoreServices {
    std::unique_ptr<IClock> clock;
    std::unique_ptr<ILogger> logger;
    std::unique_ptr<IEventBus> eventBus;
};

/// One SDK instance: configuration, shared services and initialised modules.
/// Knows no concrete module or adapter type (that is the composition root's job).
///
/// Lifetime: modules are shut down and destroyed before the services they were given.
class SdkCore final {
  public:
    /// Fails with InvalidArgument if a service is missing, or ModuleInitFailed if a module fails.
    [[nodiscard]] static Result<std::unique_ptr<SdkCore>>
    create(SdkConfig config, CoreServices services, ModuleRegistry modules);

    ~SdkCore();
    SdkCore(const SdkCore &) = delete;
    SdkCore &operator=(const SdkCore &) = delete;
    SdkCore(SdkCore &&) = delete;
    SdkCore &operator=(SdkCore &&) = delete;

    [[nodiscard]] const SdkConfig &config() const noexcept {
        return config_;
    }
    [[nodiscard]] bool hasModule(std::string_view name) const noexcept;

    [[nodiscard]] IClock &clock() const noexcept {
        return *services_.clock;
    }
    [[nodiscard]] ILogger &logger() const noexcept {
        return *services_.logger;
    }
    [[nodiscard]] IEventBus &eventBus() const noexcept {
        return *services_.eventBus;
    }

  private:
    SdkCore(SdkConfig config, CoreServices services, ModuleRegistry modules);

    // Declaration order matters: services_ must be destroyed after modules_.
    SdkConfig config_;
    CoreServices services_;
    ModuleRegistry modules_;
};

} // namespace fiaisdk::core
