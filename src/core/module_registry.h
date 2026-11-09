#pragma once

#include "core/module.h"
#include "kernel/result.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace fiaisdk::core {

/// Owns modules and drives their lifecycle. Not thread-safe for mutation; lookups (has) are safe
/// to call concurrently once initialisation finished.
class ModuleRegistry final {
  public:
    ModuleRegistry() = default;
    ~ModuleRegistry();

    ModuleRegistry(const ModuleRegistry &) = delete;
    ModuleRegistry &operator=(const ModuleRegistry &) = delete;
    ModuleRegistry(ModuleRegistry &&other) noexcept;
    ModuleRegistry &operator=(ModuleRegistry &&other) noexcept;

    /// Fails with InvalidArgument for a null module, an empty or duplicate name, or when called
    /// after initializeAll().
    [[nodiscard]] kernel::Result<void> add(std::unique_ptr<Module> module);

    /// Initialise all modules in registration order. On the first failure, already initialised
    /// modules are shut down in reverse order and the failing module's error is returned.
    [[nodiscard]] kernel::Result<void> initializeAll();

    /// Shut down initialised modules in reverse order. Idempotent.
    void shutdownAll() noexcept;

    [[nodiscard]] bool has(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept {
        return modules_.size();
    }

  private:
    std::vector<std::unique_ptr<Module>> modules_;
    std::size_t initializedCount_ = 0;
    bool initialized_ = false;
};

} // namespace fiaisdk::core
