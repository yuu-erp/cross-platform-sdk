// SPDX-License-Identifier: UNLICENSED
#pragma once

// Header-only C++17 wrapper over include/fiaisdk/network.h (ADR-0002).

#include <fiaisdk/cpp/sdk.hpp>
#include <fiaisdk/network.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace fiaisdk {

enum class NetworkState { Disconnected, Connected };

struct NetworkStatus {
    NetworkState state = NetworkState::Disconnected;
    std::chrono::milliseconds latency{0};
    std::chrono::system_clock::time_point connectedAt{};
};

/// View of the network module of one Sdk. Does not own anything: must not outlive the Sdk.
/// All methods throw fiaisdk::Error on failure (status() also on a moved-from Sdk).
class Network {
  public:
    explicit Network(const Sdk &sdk) noexcept : sdk_(sdk.native()) {}

    /// Blocks up to `timeout`. Throws Error(FIAISDK_STATUS_UNAVAILABLE) if the node does not
    /// answer, Error(FIAISDK_STATUS_INVALID_ARGUMENT) for a malformed or insecure URL.
    void connect(std::string_view url,
                 std::chrono::milliseconds timeout = std::chrono::seconds(5)) const {
        const std::string terminated(url);
        throwIfError(fiaisdk_network_connect(sdk_, terminated.c_str(), toTimeoutMs(timeout)));
    }

    void disconnect() const {
        throwIfError(fiaisdk_network_disconnect(sdk_));
    }

    [[nodiscard]] NetworkStatus status() const {
        fiaisdk_network_status raw = FIAISDK_NETWORK_STATUS_INIT;
        throwIfError(fiaisdk_network_get_status(sdk_, &raw));
        NetworkStatus out;
        out.state = raw.state == FIAISDK_NETWORK_STATE_CONNECTED ? NetworkState::Connected
                                                                 : NetworkState::Disconnected;
        out.latency = std::chrono::milliseconds(raw.latency_ms);
        out.connectedAt = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::milliseconds(raw.connected_at_unix_ms)));
        return out;
    }

    /// Full URL of the current endpoint ("" when disconnected). May contain secrets.
    [[nodiscard]] std::string endpoint() const {
        std::size_t size = 0;
        fiaisdk_status status = fiaisdk_network_get_endpoint(sdk_, nullptr, &size);
        // The endpoint can change between the two calls: retry while the buffer is too small.
        for (;;) {
            if (status != FIAISDK_STATUS_BUFFER_TOO_SMALL && status != FIAISDK_STATUS_OK) {
                throwIfError(status);
            }
            std::string buffer(size, '\0');
            status = fiaisdk_network_get_endpoint(sdk_, buffer.data(), &size);
            if (status == FIAISDK_STATUS_OK) {
                buffer.resize(size - 1); // drop the NUL
                return buffer;
            }
        }
    }

  private:
    static uint32_t toTimeoutMs(std::chrono::milliseconds timeout) noexcept {
        if (timeout.count() <= 0) {
            return 0; // rejected by the SDK as INVALID_ARGUMENT
        }
        if (timeout.count() > static_cast<std::chrono::milliseconds::rep>(UINT32_MAX)) {
            return UINT32_MAX; // rejected by the SDK as INVALID_ARGUMENT
        }
        return static_cast<uint32_t>(timeout.count());
    }

    fiaisdk_sdk *sdk_;
};

} // namespace fiaisdk
