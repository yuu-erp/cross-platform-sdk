#include "core/module_registry.h"

#include <string>
#include <utility>

namespace fiaisdk::core {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

ModuleRegistry::~ModuleRegistry() {
    shutdownAll();
}

ModuleRegistry::ModuleRegistry(ModuleRegistry &&other) noexcept
    : modules_(std::move(other.modules_)),
      initializedCount_(std::exchange(other.initializedCount_, 0)),
      initialized_(std::exchange(other.initialized_, false)) {}

ModuleRegistry &ModuleRegistry::operator=(ModuleRegistry &&other) noexcept {
    if (this != &other) {
        shutdownAll();
        modules_ = std::move(other.modules_);
        initializedCount_ = std::exchange(other.initializedCount_, 0);
        initialized_ = std::exchange(other.initialized_, false);
    }
    return *this;
}

Result<void> ModuleRegistry::add(std::unique_ptr<Module> module) {
    if (initialized_) {
        return Error{ErrorCode::InvalidArgument, "modules cannot be added after initialization"};
    }
    if (!module) {
        return Error{ErrorCode::InvalidArgument, "module is null"};
    }
    const std::string_view name = module->name();
    if (name.empty()) {
        return Error{ErrorCode::InvalidArgument, "module name is empty"};
    }
    if (has(name)) {
        return Error{ErrorCode::InvalidArgument, "duplicate module name: " + std::string(name)};
    }
    modules_.push_back(std::move(module));
    return {};
}

Result<void> ModuleRegistry::initializeAll() {
    if (initialized_) {
        return {};
    }
    for (const auto &module : modules_) {
        auto result = module->initialize();
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
