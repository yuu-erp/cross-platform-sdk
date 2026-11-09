// C ABI implementation for include/fiaisdk/network.h.

#include "bindings/c_api/handles.h"
#include "bindings/c_api/status_mapping.h"

#include <fiaisdk/network.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>

// The public layout is identical on every ABI (LP64, LLP64, ILP32): checked at compile time
// on every platform CI builds, in addition to tests/abi/network_abi_test.c.
static_assert(sizeof(fiaisdk_network_status) == 24);
static_assert(offsetof(fiaisdk_network_status, struct_size) == 0);
static_assert(offsetof(fiaisdk_network_status, state) == 4);
static_assert(offsetof(fiaisdk_network_status, connected_at_unix_ms) == 8);
static_assert(offsetof(fiaisdk_network_status, latency_ms) == 16);
static_assert(offsetof(fiaisdk_network_status, reserved0) == 20);
static_assert(FIAISDK_NETWORK_STATUS_V1_SIZE == 24);

namespace {

using fiaisdk::bindings::statusFromCurrentException;
using fiaisdk::bindings::toStatus;

/// Write `value` into the caller's struct only if the whole field fits in `callerSize` bytes.
template <class Field>
void writeField(std::span<unsigned char> callerBytes, std::size_t offset, Field value) noexcept {
    static_assert(std::is_trivially_copyable_v<Field>);
    if (callerBytes.size() >= offset + sizeof(Field)) {
        const auto raw = std::bit_cast<std::array<unsigned char, sizeof(Field)>>(value);
        std::copy(raw.begin(), raw.end(), callerBytes.subspan(offset, sizeof(Field)).begin());
    }
}

} // namespace

extern "C" {

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_connect(fiaisdk_sdk *sdk,
                                                                const char *endpoint_url,
                                                                uint32_t timeout_ms) {
    if (sdk == nullptr || endpoint_url == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.network == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        auto result =
            sdk->apis.network->connect(endpoint_url, std::chrono::milliseconds(timeout_ms));
        return result ? FIAISDK_STATUS_OK : toStatus(result.error());
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_disconnect(fiaisdk_sdk *sdk) {
    if (sdk == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.network == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        auto result = sdk->apis.network->disconnect();
        return result ? FIAISDK_STATUS_OK : toStatus(result.error());
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_network_get_status(const fiaisdk_sdk *sdk, fiaisdk_network_status *inout_status) {
    if (sdk == nullptr || inout_status == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    std::uint32_t callerSize = 0;
    std::memcpy(&callerSize, inout_status, sizeof(callerSize));
    if (callerSize < FIAISDK_NETWORK_STATUS_V1_SIZE) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.network == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        const auto current = sdk->apis.network->status();

        std::int32_t state = FIAISDK_NETWORK_STATE_DISCONNECTED;
        std::int64_t connectedAt = 0;
        std::uint32_t latency = 0;
        if (current) {
            state = FIAISDK_NETWORK_STATE_CONNECTED;
            connectedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
                              current->connectedAt.time_since_epoch())
                              .count();
            latency = static_cast<std::uint32_t>(
                std::clamp<std::int64_t>(current->latency.count(), 0, UINT32_MAX));
        }

        // Byte view of the caller's struct: only offsets inside callerSize are written.
        auto *raw = reinterpret_cast<unsigned char *>( // NOLINT(*-pro-type-reinterpret-cast)
            inout_status);
        const std::span<unsigned char> bytes{raw, callerSize};
        writeField(bytes, offsetof(fiaisdk_network_status, state), state);
        writeField(bytes, offsetof(fiaisdk_network_status, connected_at_unix_ms), connectedAt);
        writeField(bytes, offsetof(fiaisdk_network_status, latency_ms), latency);
        writeField(bytes, offsetof(fiaisdk_network_status, reserved0), std::uint32_t{0});
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_get_endpoint(const fiaisdk_sdk *sdk,
                                                                     char *buffer,
                                                                     size_t *inout_size) {
    if (inout_size == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    const std::size_t capacity = *inout_size;
    *inout_size = 0; // every error path below reports 0
    if (sdk == nullptr || (buffer == nullptr && capacity != 0)) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.network == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        const auto current = sdk->apis.network->status();
        const std::string url = current ? current->endpoint.url() : std::string();

        const std::size_t required = url.size() + 1;
        *inout_size = required;
        if (capacity < required || buffer == nullptr) {
            return FIAISDK_STATUS_BUFFER_TOO_SMALL;
        }
        std::memcpy(buffer, url.c_str(), required);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        *inout_size = 0;
        return statusFromCurrentException();
    }
}

} // extern "C"
