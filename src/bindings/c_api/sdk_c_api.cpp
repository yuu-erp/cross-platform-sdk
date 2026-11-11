// C ABI implementation for include/fiaisdk/sdk.h.
// Every exported function validates arguments, initialises out-params, and catches everything.

#include "bindings/c_api/handles.h"
#include "bindings/c_api/status_mapping.h"
#include "bootstrap/composition_root.h"

#include <fiaisdk/sdk.h>
#include <fiaisdk/version.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace {

using fiaisdk::bindings::statusFromCurrentException;
using fiaisdk::bindings::toStatus;

/// Copy one field from the caller's bytes, only if the whole field lies inside them.
template <class Field>
void copyField(std::span<const unsigned char> callerBytes, std::size_t offset,
               Field &out) noexcept {
    static_assert(std::is_trivially_copyable_v<Field>);
    if (callerBytes.size() >= offset + sizeof(Field)) {
        std::array<unsigned char, sizeof(Field)> raw{};
        const auto field = callerBytes.subspan(offset, sizeof(Field));
        std::copy(field.begin(), field.end(), raw.begin());
        out = std::bit_cast<Field>(raw);
    }
}

/// Read a caller config of any size >= V1 into the library's current layout. Fields the caller
/// does not have (older header) stay zero/NULL; trailing fields we do not know (newer header) are
/// ignored. Never reads outside the caller's struct_size bytes.
fiaisdk_sdk_config readConfig(const void *callerConfig, std::uint32_t callerSize) noexcept {
    const std::span<const unsigned char> bytes{static_cast<const unsigned char *>(callerConfig),
                                               callerSize};
    fiaisdk_sdk_config local{};
    local.struct_size = static_cast<std::uint32_t>(sizeof(fiaisdk_sdk_config));
    copyField(bytes, offsetof(fiaisdk_sdk_config, application_name), local.application_name);
    copyField(bytes, offsetof(fiaisdk_sdk_config, application_version), local.application_version);
    return local;
}

} // namespace

extern "C" {

FIAISDK_API const char *FIAISDK_CALL fiaisdk_version(void) {
    return FIAISDK_VERSION_STRING;
}

FIAISDK_API uint32_t FIAISDK_CALL fiaisdk_version_number(void) {
    return FIAISDK_VERSION_NUMBER;
}

FIAISDK_API const char *FIAISDK_CALL fiaisdk_status_message(fiaisdk_status status) {
    switch (status) {
    case FIAISDK_STATUS_OK:
        return "Success";
    case FIAISDK_STATUS_INVALID_ARGUMENT:
        return "Invalid argument";
    case FIAISDK_STATUS_NOT_INITIALIZED:
        return "SDK is not initialized";
    case FIAISDK_STATUS_MODULE_NOT_FOUND:
        return "Module not found";
    case FIAISDK_STATUS_BUFFER_TOO_SMALL:
        return "Buffer too small";
    case FIAISDK_STATUS_OUT_OF_MEMORY:
        return "Out of memory";
    case FIAISDK_STATUS_UNAVAILABLE:
        return "Unavailable (resource unreachable, locked or timed out)";
    case FIAISDK_STATUS_NOT_FOUND:
        return "Not found";
    case FIAISDK_STATUS_ALREADY_EXISTS:
        return "Already exists";
    case FIAISDK_STATUS_AUTHENTICATION_FAILED:
        return "Authentication failed (key or credential rejected)";
    case FIAISDK_STATUS_INTERNAL_ERROR:
        return "Internal error";
    default:
        return "Unknown status";
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_sdk_create(const fiaisdk_sdk_config *config,
                                                           fiaisdk_sdk **out_sdk) {
    if (out_sdk == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    *out_sdk = nullptr;
    if (config == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    std::uint32_t callerSize = 0; // first field of every version
    std::memcpy(&callerSize, config, sizeof(callerSize));
    if (callerSize < FIAISDK_SDK_CONFIG_V1_SIZE) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }

    try {
        const fiaisdk_sdk_config raw = readConfig(config, callerSize);

        fiaisdk::core::SdkConfig sdkConfig;
        if (raw.application_name != nullptr) {
            sdkConfig.applicationName = raw.application_name;
        }
        if (raw.application_version != nullptr) {
            sdkConfig.applicationVersion = raw.application_version;
        }

        auto composed = fiaisdk::bootstrap::composeSdk(std::move(sdkConfig));
        if (!composed) {
            return toStatus(composed.error());
        }

        auto parts = std::move(composed).value();
        auto handle = std::make_unique<fiaisdk_sdk>();
        handle->core = std::move(parts.core);
        handle->apis = parts.apis;
        *out_sdk = handle.release();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API void FIAISDK_CALL fiaisdk_sdk_destroy(fiaisdk_sdk *sdk) {
    // ~SdkCore and module shutdown hooks are noexcept, so nothing can escape here.
    delete sdk; // NOLINT(cppcoreguidelines-owning-memory): handle ownership crosses the C ABI
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_sdk_has_module(const fiaisdk_sdk *sdk,
                                                               const char *module_name,
                                                               uint8_t *out_available) {
    if (out_available != nullptr) {
        *out_available = 0;
    }
    if (sdk == nullptr || !sdk->core || module_name == nullptr || out_available == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        *out_available = static_cast<uint8_t>(sdk->core->hasModule(module_name) ? 1U : 0U);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

} // extern "C"
