#pragma once

#include "core/domain/domain_event.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace fiaisdk::network {

/// Published after a connection is established. Carries the endpoint origin only
/// ("https://host:port"), never path/query, which may contain API keys.
///
/// sequence(): increases with every connection state change of the SDK instance. Events are
/// delivered after the state changed and may arrive out of order across threads: ignore an
/// event whose sequence is lower than the last one you handled (or re-read the status).
class NetworkConnected final : public core::DomainEvent {
  public:
    static constexpr std::string_view kName = "network.connected";

    NetworkConnected(core::Timestamp at, std::uint64_t sequence, std::string origin,
                     std::chrono::milliseconds latency) noexcept
        : DomainEvent(at), sequence_(sequence), origin_(std::move(origin)), latency_(latency) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] const std::string &origin() const noexcept {
        return origin_;
    }
    [[nodiscard]] std::chrono::milliseconds latency() const noexcept {
        return latency_;
    }
    [[nodiscard]] std::uint64_t sequence() const noexcept {
        return sequence_;
    }

  private:
    std::uint64_t sequence_;
    std::string origin_;
    std::chrono::milliseconds latency_;
};

/// Published after a connection is closed.
class NetworkDisconnected final : public core::DomainEvent {
  public:
    enum class Reason : std::uint8_t {
        Requested,         ///< the host application asked to disconnect
        SwitchingEndpoint, ///< replaced by a connection to another endpoint
        PeerClosed,        ///< the transport detected that the peer closed the connection
        Shutdown,          ///< the SDK instance is being destroyed
    };

    static constexpr std::string_view kName = "network.disconnected";

    NetworkDisconnected(core::Timestamp at, std::uint64_t sequence, std::string origin,
                        Reason reason) noexcept
        : DomainEvent(at), sequence_(sequence), origin_(std::move(origin)), reason_(reason) {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] const std::string &origin() const noexcept {
        return origin_;
    }
    [[nodiscard]] Reason reason() const noexcept {
        return reason_;
    }
    [[nodiscard]] std::uint64_t sequence() const noexcept {
        return sequence_;
    }

  private:
    std::uint64_t sequence_;
    std::string origin_;
    Reason reason_;
};

} // namespace fiaisdk::network
