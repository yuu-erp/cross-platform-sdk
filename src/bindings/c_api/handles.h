#pragma once

#include "core/sdk_core.h"

#include <memory>

// Definitions of the opaque handle types declared in include/fiaisdk/types.h.
// Visible only to the C ABI implementation.
struct fiaisdk_sdk {
    std::unique_ptr<fiaisdk::core::SdkCore> core;
};
