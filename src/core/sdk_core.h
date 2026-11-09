#pragma once

#include "core/module_registry.h"
#include "kernel/result.h"

#include <memory>
#include <string>
#include <string_view>

namespace fiaisdk::core {

/// Internal, C++-typed copy of fiaisdk_sdk_config.
struct SdkConfig {
    std::string applicationName;
    std::string applicationVersion;
};

/// One SDK instance: configuration + initialised modules. Knows no concrete module types.
class SdkCore final {
  public:
    /// Initialises every module in `modules`; fails if any module fails.
    [[nodiscard]] static kernel::Result<std::unique_ptr<SdkCore>> create(SdkConfig config,
                                                                         ModuleRegistry modules);

    ~SdkCore();
    SdkCore(const SdkCore &) = delete;
    SdkCore &operator=(const SdkCore &) = delete;
    SdkCore(SdkCore &&) = delete;
    SdkCore &operator=(SdkCore &&) = delete;

    [[nodiscard]] const SdkConfig &config() const noexcept {
        return config_;
    }
    [[nodiscard]] bool hasModule(std::string_view name) const noexcept;

  private:
    SdkCore(SdkConfig config, ModuleRegistry modules);

    SdkConfig config_;
    ModuleRegistry modules_;
};

} // namespace fiaisdk::core
