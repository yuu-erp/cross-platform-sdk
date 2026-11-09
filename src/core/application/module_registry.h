#pragma once

#include "core/application/module.h"
#include "core/application/module_context.h"
#include "core/domain/result.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace fiaisdk::core {

/// Owns modules and drives their lifecycle. Mutation is not thread-safe; has() is safe to call
/// concurrently once initialisation finished.
class ModuleRegistry final {
  public:
    ModuleRegistry() = default;
    ~ModuleRegistry();

    ModuleRegistry(const ModuleRegistry &) = delete;
    ModuleRegistry &operator=(const ModuleRegistry &) = delete;
    ModuleRegistry(ModuleRegistry &&other) noexcept;
    ModuleRegistry &operator=(ModuleRegistry &&other) noexcept;

    /// Fails with InvalidArgument for a null module, an empty or duplicate name, or when called
    /// after initializeAll() or shutdownAll().
    [[nodiscard]] Result<void> add(std::unique_ptr<Module> module);

    /// Initialise all modules in registration order. On the first failure, already initialised
    /// modules are shut down in reverse order and ModuleInitFailed is returned. Fails with
    /// InvalidArgument after shutdownAll() (a registry is initialised at most once).
    [[nodiscard]] Result<void> initializeAll(const ModuleContext &context);

    /// Shut down initialised modules in reverse order. Idempotent and final.
    void shutdownAll() noexcept;

    [[nodiscard]] bool has(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept {
        return modules_.size();
    }

  private:
    std::vector<std::unique_ptr<Module>> modules_;
    std::size_t initializedCount_ = 0;
    bool initialized_ = false;
    bool finished_ = false; // shutdownAll() ran: no further add/initialize
};

} // namespace fiaisdk::core
