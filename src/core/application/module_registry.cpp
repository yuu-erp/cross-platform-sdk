#include "core/application/module_registry.h"

#include <string>
#include <utility>

namespace fiaisdk::core {

ModuleRegistry::~ModuleRegistry() {
    shutdownAll();
}

ModuleRegistry::ModuleRegistry(ModuleRegistry &&other) noexcept
    : modules_(std::move(other.modules_)),
      initializedCount_(std::exchange(other.initializedCount_, 0)),
      initialized_(std::exchange(other.initialized_, false)),
      finished_(std::exchange(other.finished_, false)) {}

ModuleRegistry &ModuleRegistry::operator=(ModuleRegistry &&other) noexcept {
    if (this != &other) {
        shutdownAll();
        modules_ = std::move(other.modules_);
        initializedCount_ = std::exchange(other.initializedCount_, 0);
        initialized_ = std::exchange(other.initialized_, false);
        finished_ = std::exchange(other.finished_, false);
    }
    return *this;
}

Result<void> ModuleRegistry::add(std::unique_ptr<Module> module) {
    if (initialized_ || finished_) {
        return Error::invalidArgument("modules cannot be added after initialization");
    }
    if (!module) {
        return Error::invalidArgument("module is null");
    }
    const std::string_view name = module->name();
    if (name.empty()) {
        return Error::invalidArgument("module name is empty");
    }
    if (has(name)) {
        return Error::invalidArgument("duplicate module name: " + std::string(name));
    }
    modules_.push_back(std::move(module));
    return {};
}

Result<void> ModuleRegistry::initializeAll(const ModuleContext &context) {
    if (finished_) {
        return Error::invalidArgument("registry was shut down; create a new one");
    }
    if (initialized_) {
        return {};
    }
    for (const auto &module : modules_) {
        auto result = module->initialize(context);
        if (!result) {
            shutdownAll();
            return Error{ErrorCode::ModuleInitFailed,
                         std::string(module->name()) + ": " + result.error().message};
        }
        ++initializedCount_;
    }
    initialized_ = true;
    return {};
}

void ModuleRegistry::shutdownAll() noexcept {
    while (initializedCount_ > 0) {
        --initializedCount_;
        modules_[initializedCount_]->shutdown();
    }
    if (initialized_ || !modules_.empty()) {
        finished_ = true;
    }
    initialized_ = false;
}

bool ModuleRegistry::has(std::string_view name) const noexcept {
    for (const auto &module : modules_) {
        if (module->name() == name) {
            return true;
        }
    }
    return false;
}

} // namespace fiaisdk::core
