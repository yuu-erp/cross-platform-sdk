#include "bindings/c_api/handles.h"
#include "bindings/c_api/status_mapping.h"
#include "core/domain/secure_bytes.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fiaisdk/wallet.h>
#include <memory>
#include <span>

static_assert(sizeof(fiaisdk_wallet_info) == FIAISDK_WALLET_INFO_V1_SIZE);
static_assert(offsetof(fiaisdk_wallet_info, bls_public_key) == 4);
static_assert(offsetof(fiaisdk_wallet_info, secp256k1_public_key) == 52);
static_assert(offsetof(fiaisdk_wallet_info, address) == 117);
static_assert(offsetof(fiaisdk_wallet_info, bls_address) == 137);
static_assert(offsetof(fiaisdk_wallet_info, reserved0) == 157);

extern "C" {
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_create(const fiaisdk_sdk *sdk,
                                                              fiaisdk_wallet **out_wallet) {
    if (out_wallet == nullptr)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    *out_wallet = nullptr;
    if (sdk == nullptr)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    if (sdk->apis.wallet == nullptr)
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    try {
        auto created = sdk->apis.wallet->create();
        if (!created)
            return fiaisdk::bindings::toStatus(created.error());
        auto handle = std::make_unique<fiaisdk_wallet>();
        handle->value = std::move(created).value();
        *out_wallet = handle.release();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fiaisdk::bindings::statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_import(
    const fiaisdk_sdk *sdk, const uint8_t *bls_key, size_t bls_key_size,
    const uint8_t *secp256k1_key, size_t secp256k1_key_size, fiaisdk_wallet **out_wallet) {
    if (out_wallet == nullptr)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    *out_wallet = nullptr;
    if (sdk == nullptr || bls_key == nullptr || secp256k1_key == nullptr || bls_key_size != 32 ||
        secp256k1_key_size != 32)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    if (sdk->apis.wallet == nullptr)
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    try {
        auto imported =
            sdk->apis.wallet->import({bls_key, bls_key_size}, {secp256k1_key, secp256k1_key_size});
        if (!imported)
            return fiaisdk::bindings::toStatus(imported.error());
        auto handle = std::make_unique<fiaisdk_wallet>();
        handle->value = std::move(imported).value();
        *out_wallet = handle.release();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fiaisdk::bindings::statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_get_info(const fiaisdk_wallet *wallet,
                                                                fiaisdk_wallet_info *inout_info) {
    if (inout_info == nullptr)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    std::uint32_t size = 0;
    std::memcpy(&size, inout_info, sizeof(size));
    if (size < FIAISDK_WALLET_INFO_V1_SIZE)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    fiaisdk_wallet_info local = FIAISDK_WALLET_INFO_INIT;
    const auto writePayload = [&] {
        // Copy only the known payload. struct_size and a future caller's tail are untouched.
        auto *destination =
            reinterpret_cast<unsigned char *>(inout_info); // NOLINT(*-pro-type-reinterpret-cast)
        const auto *source =
            reinterpret_cast<const unsigned char *>(&local); // NOLINT(*-pro-type-reinterpret-cast)
        std::memcpy(destination + sizeof(size), source + sizeof(size),
                    sizeof(local) - sizeof(size)); // NOLINT(*-pro-bounds-pointer-arithmetic)
    };
    if (wallet == nullptr || !wallet->value) {
        writePayload();
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        const auto &info = wallet->value->info();
        std::copy(info.blsPublicKey.begin(), info.blsPublicKey.end(), local.bls_public_key);
        std::copy(info.secpPublicKey.begin(), info.secpPublicKey.end(), local.secp256k1_public_key);
        std::copy(info.address.begin(), info.address.end(), local.address);
        std::copy(info.blsAddress.begin(), info.blsAddress.end(), local.bls_address);
        writePayload();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        writePayload();
        return fiaisdk::bindings::statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_sign(const fiaisdk_sdk *sdk,
                                                            const fiaisdk_wallet *wallet,
                                                            int32_t scheme, const uint8_t *data,
                                                            size_t data_size, uint8_t *output,
                                                            size_t output_size) {
    const std::size_t required = scheme == FIAISDK_CRYPTO_SCHEME_BLS12381 ? 96 : 65;
    if (scheme != FIAISDK_CRYPTO_SCHEME_BLS12381 && scheme != FIAISDK_CRYPTO_SCHEME_SECP256K1)
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    const auto fail = [&](fiaisdk_status status) {
        if (output != nullptr && output_size >= required)
            fiaisdk::core::secureZero(std::span<std::uint8_t>(output, required));
        return status;
    };
    if (sdk == nullptr || wallet == nullptr || !wallet->value || output == nullptr ||
        (data == nullptr && data_size != 0))
        return fail(FIAISDK_STATUS_INVALID_ARGUMENT);
    if (output_size < required)
        return FIAISDK_STATUS_BUFFER_TOO_SMALL;
    if (sdk->apis.wallet == nullptr)
        return fail(FIAISDK_STATUS_MODULE_NOT_FOUND);
    try {
        auto signature = sdk->apis.wallet->sign(
            *wallet->value, static_cast<fiaisdk::crypto::KeyScheme>(scheme), {data, data_size});
        if (!signature)
            return fail(fiaisdk::bindings::toStatus(signature.error()));
        if (signature.value().size() != required)
            return fail(FIAISDK_STATUS_INTERNAL_ERROR);
        std::copy(signature.value().begin(), signature.value().end(), output);
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return fail(fiaisdk::bindings::statusFromCurrentException());
    }
}

FIAISDK_API void FIAISDK_CALL fiaisdk_wallet_destroy(fiaisdk_wallet *wallet) {
    delete wallet; // NOLINT(cppcoreguidelines-owning-memory)
}
} // extern "C"
