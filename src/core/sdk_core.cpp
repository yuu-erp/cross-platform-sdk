#include "core/sdk_core.h"

#include <utility>

namespace fiaisdk::core {

kernel::Result<std::unique_ptr<SdkCore>> SdkCore::create(SdkConfig config, ModuleRegistry modules) {
    auto initResult = modules.initializeAll();
    if (!initResult) {
        return initResult.error();
    }
    // Private constructor: std::make_unique cannot be used.
    return std::unique_ptr<SdkCore>(new SdkCore(std::move(config), std::move(modules)));
}

SdkCore::SdkCore(SdkConfig config, ModuleRegistry modules)
    : config_(std::move(config)), modules_(std::move(modules)) {}

SdkCore::~SdkCore() {
    modules_.shutdownAll();
}

bool SdkCore::hasModule(std::string_view name) const noexcept {
    return modules_.has(name);
}

} // namespace fiaisdk::core
