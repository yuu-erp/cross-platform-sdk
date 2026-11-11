#include "bindings/c_api/handles.h"
#include "bindings/c_api/status_mapping.h"

#include <algorithm>
#include <fiaisdk/wallet.h>
#include <string_view>

struct fiaisdk_wallet_store {
    std::unique_ptr<fiaisdk::wallet::IWalletStore> value;
};

namespace {
using fiaisdk::bindings::statusFromCurrentException;
using fiaisdk::bindings::toStatus;

fiaisdk::wallet::WalletAddress convertAddress(const fiaisdk_wallet_address &address) {
    return std::to_array(address.bytes);
}
} // namespace

extern "C" {

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_store_open(
    const char *path, const uint8_t *key, size_t key_size, fiaisdk_wallet_store **out_store) {
    if (out_store == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    *out_store = nullptr;
    if (path == nullptr || *path == '\0' || std::string_view(path) == ":memory:" ||
        key == nullptr || key_size != 32) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        auto handle = std::make_unique<fiaisdk_wallet_store>();
        auto opened = fiaisdk::bootstrap::openWalletStore(
            path, std::span<const std::uint8_t, 32>(key, key_size));
        if (!opened) {
            return toStatus(opened.error());
        }
        handle->value = std::move(opened).value();
        *out_store = handle.release();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

// NOLINTNEXTLINE(misc-const-correctness)
FIAISDK_API void FIAISDK_CALL fiaisdk_wallet_store_close(fiaisdk_wallet_store *store) {
    delete store; // NOLINT(cppcoreguidelines-owning-memory)
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_store_save(fiaisdk_wallet_store *store,
                                                                  const fiaisdk_wallet *wallet) {
    if (store == nullptr || wallet == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        auto saved = store->value->save(*wallet->value);
        return saved ? FIAISDK_STATUS_OK : toStatus(saved.error());
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_wallet_store_load(fiaisdk_wallet_store *store, const fiaisdk_sdk *sdk,
                          const fiaisdk_wallet_address *address, fiaisdk_wallet **out_wallet) {
    if (out_wallet == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    *out_wallet = nullptr;
    if (store == nullptr || sdk == nullptr || address == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    if (sdk->apis.wallet == nullptr) {
        return FIAISDK_STATUS_MODULE_NOT_FOUND;
    }
    try {
        auto loaded = sdk->apis.wallet->load(*store->value, convertAddress(*address));
        if (!loaded) {
            return toStatus(loaded.error());
        }
        auto handle = std::make_unique<fiaisdk_wallet>();
        handle->value = std::move(loaded).value();
        *out_wallet = handle.release();
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_store_list(fiaisdk_wallet_store *store,
                                                                  fiaisdk_wallet_address *addresses,
                                                                  size_t *inout_count) {
    if (store == nullptr || inout_count == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        auto listed = store->value->addresses();
        if (!listed) {
            return toStatus(listed.error());
        }
        const auto &result = listed.value();
        const auto capacity = addresses == nullptr ? 0 : *inout_count;
        *inout_count = result.size();
        if (addresses == nullptr) {
            return FIAISDK_STATUS_OK;
        }
        if (capacity < result.size()) {
            return FIAISDK_STATUS_BUFFER_TOO_SMALL;
        }
        auto output = std::span(addresses, result.size());
        for (std::size_t i = 0; i < result.size(); ++i) {
            std::ranges::copy(result[i], std::span(output[i].bytes).begin());
        }
        return FIAISDK_STATUS_OK;
    } catch (...) {
        return statusFromCurrentException();
    }
}

FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_wallet_store_remove(fiaisdk_wallet_store *store, const fiaisdk_wallet_address *address) {
    if (store == nullptr || address == nullptr) {
        return FIAISDK_STATUS_INVALID_ARGUMENT;
    }
    try {
        auto removed = store->value->remove(convertAddress(*address));
        return removed ? FIAISDK_STATUS_OK : toStatus(removed.error());
    } catch (...) {
        return statusFromCurrentException();
    }
}

} // extern "C"
