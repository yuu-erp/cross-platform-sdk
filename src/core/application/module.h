#pragma once

#include "core/application/module_context.h"
#include "core/domain/result.h"

#include <string_view>

namespace fiaisdk::core {

/// Contract every SDK module implements so core can manage its lifecycle.
/// Core never sees module business logic; it only calls these hooks.
class Module {
  public:
    Module() = default;
    virtual ~Module() = default;

    Module(const Module &) = delete;
    Module &operator=(const Module &) = delete;
    Module(Module &&) = delete;
    Module &operator=(Module &&) = delete;

    /// Stable, unique, lower_snake_case identifier (e.g. "wallet"). Exposed through the C ABI.
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Called once, in registration order, before the SDK handle is returned to the caller.
    /// Subscribe to events and keep references to services from `context` here.
    [[nodiscard]] virtual Result<void> initialize(const ModuleContext &context) {
        static_cast<void>(context);
        return {};
    }

    /// Called once, in reverse order, only for modules whose initialize() succeeded.
    /// Drop subscriptions and stop work here; shared services are still alive.
    virtual void shutdown() noexcept {}
};

} // namespace fiaisdk::core
