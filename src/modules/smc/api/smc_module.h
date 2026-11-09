#pragma once

#include "core/application/module.h"

#include <string_view>

namespace fiaisdk::smc {

/// Lifecycle entry point of the smc module. Placeholder: no use cases yet.
class SmcModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "smc";

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
};

} // namespace fiaisdk::smc
