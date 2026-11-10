#pragma once

#include "bootstrap/composition_root.h"
#include "core/application/sdk_core.h"

#include <memory>

// Definitions of the opaque handle types declared in include/fiaisdk/types.h.
// Visible only to the C ABI implementation.
struct fiaisdk_sdk {
    std::unique_ptr<fiaisdk::core::SdkCore> core;
    /// Non-owning; the facades live inside `core`'s modules.
    fiaisdk::bootstrap::ModuleApis apis;
};

// Keys and metadata are owned independently of SDK lifetime; signing takes an explicit SDK.
struct fiaisdk_wallet {
    std::unique_ptr<fiaisdk::wallet::Wallet> value;
};
