#pragma once

#include "core/module.h"

#include <string_view>

namespace fiaisdk::network {

/// Lifecycle entry point of the network module. Placeholder: no use cases yet.
class NetworkModule final : public core::Module {
  public:
    static constexpr std::string_view kName = "network";

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
};

} // namespace fiaisdk::network
