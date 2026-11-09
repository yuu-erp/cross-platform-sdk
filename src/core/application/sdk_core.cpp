#include "core/application/sdk_core.h"

#include <utility>

namespace fiaisdk::core {

Result<std::unique_ptr<SdkCore>> SdkCore::create(SdkConfig config, CoreServices services,
                                                 ModuleRegistry modules) {
    if (!services.clock || !services.logger || !services.eventBus) {
        return Error::invalidArgument("core services must all be provided");
    }
    // Private constructor: std::make_unique cannot be used.
    auto core = std::unique_ptr<SdkCore>(
        new SdkCore(std::move(config), std::move(services), std::move(modules)));

    const ModuleContext context(core->clock(), core->logger(), core->eventBus());
    auto initResult = core->modules_.initializeAll(context);
    if (!initResult) {
        return initResult.error();
    }
    return core;
}

SdkCore::SdkCore(SdkConfig config, CoreServices services, ModuleRegistry modules)
    : config_(std::move(config)), services_(std::move(services)), modules_(std::move(modules)) {}

SdkCore::~SdkCore() {
    modules_.shutdownAll();
}

bool SdkCore::hasModule(std::string_view name) const noexcept {
    return modules_.has(name);
}

} // namespace fiaisdk::core
