#include "bindings/c_api/handles.h"
#include "bindings/c_api/status_mapping.h"
#include "core/domain/secure_bytes.h"

#include <fiaisdk/crypto.h>

#include <algorithm>
#include <span>

namespace {

void clearOutput(std::uint8_t *output, std::size_t size) noexcept {
    if (output != nullptr) {
        fiaisdk::core::secureZero(std::span<std::uint8_t>(output, size));
    }
}

} // namespace

extern "C" {

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_sha256(const fiaisdk_sdk *sdk,
                                                              const uint8_t *input,
                                                              size_t input_size, uint8_t *output,
                                                              size_t output_size) {
    const auto fail = [&](fiaisdk_status status) {
        if (output_size >= FIAISDK_CRYPTO_SHA256_SIZE) {
            clearOutput(output, FIAISDK_CRYPTO_SHA256_SIZE);
        }
        return status;
    };
    if (sdk == nullptr || (input == nullptr && input_size != 0) || output == nullptr) {
        return fail(FIAISDK_STATUS_INVALID_ARGUMENT);
    }
    if (output_size < FIAISDK_CRYPTO_SHA256_SIZE) {
        return FIAISDK_STATUS_BUFFER_TOO_SMALL;
    }
    if (sdk->apis.crypto == nullptr) {
        return fail(FIAISDK_STATUS_MODULE_NOT_FOUND);
    }
    try {
        auto result = sdk->apis.crypto->sha256(std::span<const std::uint8_t>(input, input_size));
        if (!result) {
            return fail(fiaisdk::bindings::toStatus(result.error()));
        }
        std::copy(result.value().begin(), result.value().end(), output);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fail(fiaisdk::bindings::statusFromCurrentException());
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_keccak256(const fiaisdk_sdk *sdk,
                                                                 const uint8_t *input,
                                                                 size_t input_size, uint8_t *output,
                                                                 size_t output_size) {
    const auto fail = [&](fiaisdk_status status) {
        if (output_size >= FIAISDK_CRYPTO_SHA256_SIZE) {
            clearOutput(output, FIAISDK_CRYPTO_SHA256_SIZE);
        }
        return status;
    };
    if (sdk == nullptr || (input == nullptr && input_size != 0) || output == nullptr) {
        return fail(FIAISDK_STATUS_INVALID_ARGUMENT);
    }
    if (output_size < FIAISDK_CRYPTO_SHA256_SIZE) {
        return FIAISDK_STATUS_BUFFER_TOO_SMALL;
    }
    if (sdk->apis.crypto == nullptr) {
        return fail(FIAISDK_STATUS_MODULE_NOT_FOUND);
    }
    try {
        auto result = sdk->apis.crypto->keccak256(std::span<const std::uint8_t>(input, input_size));
        if (!result) {
            return fail(fiaisdk::bindings::toStatus(result.error()));
        }
        std::copy(result.value().begin(), result.value().end(), output);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fail(fiaisdk::bindings::statusFromCurrentException());
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_random_bytes(const fiaisdk_sdk *sdk,
                                                                    uint8_t *output,
                                                                    size_t output_size) {
    if (output_size == 0 || output_size > FIAISDK_CRYPTO_MAX_RANDOM_SIZE) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    clearOutput(output, output_size);
    if (sdk == nullptr || output == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.crypto == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        auto result = sdk->apis.crypto->randomBytes(output_size);
        if (!result) {
            return fiaisdk::bindings::toStatus(result.error());
        }
        const auto bytes = result.value().view();
        std::copy(bytes.begin(), bytes.end(), output);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fiaisdk::bindings::statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_crypto_verify(const fiaisdk_sdk *sdk, int32_t scheme, const uint8_t *public_key,
                      size_t public_key_size, const uint8_t *data, size_t data_size,
                      const uint8_t *signature, size_t signature_size, uint8_t *out_valid) {
    if (out_valid != nullptr)
        *out_valid = 0;
    if (sdk == nullptr || public_key == nullptr || signature == nullptr || out_valid == nullptr ||
        (data == nullptr && data_size != 0))
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    if (sdk->apis.crypto == nullptr)
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    try {
        auto result = sdk->apis.crypto->verify(static_cast<fiaisdk::crypto::KeyScheme>(scheme),
                                               {public_key, public_key_size}, {data, data_size},
                                               {signature, signature_size});
        if (!result)
            return fiaisdk::bindings::toStatus(result.error());
        *out_valid = result.value() ? 1 : 0;
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fiaisdk::bindings::statusFromCurrentException();
    }
}

} // extern "C"
