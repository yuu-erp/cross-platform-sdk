#pragma once

#include "core/application/ports/i_clock.h"
#include "core/application/ports/i_event_bus.h"
#include "core/application/ports/i_logger.h"

namespace fiaisdk::core {

/// Shared services handed to every module in Module::initialize().
///
/// Lifetime: the referenced services are owned by SdkCore and outlive every module (modules are
/// shut down and destroyed first), so a module may keep the services until shutdown().
/// The ModuleContext object passed to initialize() is temporary: copy it (it is a cheap,
/// copyable handle) instead of storing a reference or pointer to it.
/// New shared services are added here (not as globals) so modules stay testable.
class ModuleContext {
  public:
    ModuleContext(IClock &clock, ILogger &logger, IEventBus &eventBus) noexcept
        : clock_(&clock), logger_(&logger), eventBus_(&eventBus) {}

    [[nodiscard]] IClock &clock() const noexcept {
        return *clock_;
    }
    [[nodiscard]] ILogger &logger() const noexcept {
        return *logger_;
    }
    [[nodiscard]] IEventBus &eventBus() const noexcept {
        return *eventBus_;
    }

  private:
    IClock *clock_;
    ILogger *logger_;
    IEventBus *eventBus_;
};

} // namespace fiaisdk::core
