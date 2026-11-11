#pragma once

#include "core/domain/result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace fiaisdk::network {

/// Address of a node the SDK talks to, e.g. "https://node.example.com:8443/rpc".
///
/// Value object: immutable, compared by value, only constructible through parse(), so every
/// Endpoint in the system is valid. Rules:
/// - scheme https, wss, http or ws (case-insensitive); http/ws only for the loopback hosts
///   localhost, 127.0.0.1 and [::1] — everything else must use TLS;
/// - tcp requires an explicit port and has no path, query or fragment;
/// - no user-info ("user:pass@host") so credentials never travel inside URLs;
/// - host: DNS name or IPv4 (ASCII labels of letters, digits, '-', 1..63 chars, no leading or
///   trailing '-'), or an IPv6 literal in brackets; parsing is locale-independent;
/// - port 1..65535, default 443 (https/wss) or 80 (http/ws);
/// - at most 2048 characters, no whitespace or control characters.
class Endpoint {
  public:
    enum class Scheme : std::uint8_t { Https, Wss, Http, Ws, Tcp };

    static constexpr std::size_t kMaxLength = 2048;

    [[nodiscard]] static core::Result<Endpoint> parse(std::string_view url);

    [[nodiscard]] Scheme scheme() const noexcept {
        return scheme_;
    }
    [[nodiscard]] const std::string &host() const noexcept {
        return host_;
    }
    [[nodiscard]] std::uint16_t port() const noexcept {
        return port_;
    }
    /// Path, query and fragment exactly as given (may hold API keys: never log it).
    [[nodiscard]] const std::string &target() const noexcept {
        return target_;
    }
    [[nodiscard]] bool isSecure() const noexcept;

    /// Normalised full URL: lower-case scheme and host, default port omitted.
    [[nodiscard]] std::string url() const;
    /// "scheme://host[:port]" without path/query: safe to log and to put in events.
    [[nodiscard]] std::string origin() const;

    friend bool operator==(const Endpoint &, const Endpoint &) = default;

  private:
    Endpoint(Scheme scheme, std::string host, std::uint16_t port, std::string target)
        : scheme_(scheme), host_(std::move(host)), port_(port), target_(std::move(target)) {}

    Scheme scheme_;
    std::string host_;
    std::uint16_t port_;
    std::string target_;
};

[[nodiscard]] std::string_view toString(Endpoint::Scheme scheme) noexcept;

} // namespace fiaisdk::network
