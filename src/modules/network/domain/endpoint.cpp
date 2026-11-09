#include "modules/network/domain/endpoint.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <utility>

namespace fiaisdk::network {

namespace {

constexpr std::size_t kMaxHostLength = 253;

// ASCII-only helpers: <cctype> depends on the host application's locale.
constexpr bool isAsciiDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}
constexpr bool isAsciiAlpha(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
constexpr bool isAsciiAlnum(char c) noexcept {
    return isAsciiDigit(c) || isAsciiAlpha(c);
}
constexpr bool isAsciiHexDigit(char c) noexcept {
    return isAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
constexpr char toLower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), toLower);
    return out;
}

std::optional<Endpoint::Scheme> parseScheme(std::string_view text) {
    const std::string scheme = lowered(text);
    if (scheme == "https") {
        return Endpoint::Scheme::Https;
    }
    if (scheme == "wss") {
        return Endpoint::Scheme::Wss;
    }
    if (scheme == "http") {
        return Endpoint::Scheme::Http;
    }
    if (scheme == "ws") {
        return Endpoint::Scheme::Ws;
    }
    return std::nullopt;
}

std::uint16_t defaultPort(Endpoint::Scheme scheme) noexcept {
    return (scheme == Endpoint::Scheme::Https || scheme == Endpoint::Scheme::Wss) ? 443 : 80;
}

/// DNS name or dotted IPv4: labels of [A-Za-z0-9-], 1..63 chars, not starting/ending with '-'.
bool isDnsOrIpv4Host(std::string_view host) noexcept {
    if (host.empty() || host.size() > kMaxHostLength) {
        return false;
    }
    constexpr std::size_t kMaxLabel = 63;
    std::size_t start = 0;
    while (start <= host.size()) {
        const auto dot = host.find('.', start);
        const auto label = host.substr(start, dot == std::string_view::npos ? std::string_view::npos
                                                                            : dot - start);
        if (label.empty() || label.size() > kMaxLabel || label.front() == '-' ||
            label.back() == '-' || !std::all_of(label.begin(), label.end(), [](char c) {
                return isAsciiAlnum(c) || c == '-';
            })) {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        start = dot + 1;
    }
    return false;
}

bool isIpv6Literal(std::string_view inside) noexcept {
    if (inside.empty() || inside.find(':') == std::string_view::npos) {
        return false;
    }
    return std::all_of(inside.begin(), inside.end(),
                       [](char c) { return isAsciiHexDigit(c) || c == ':' || c == '.'; });
}

/// Hosts that are loopback by definition, independent of any resolver. "*.localhost" names are
/// deliberately NOT included: some resolvers forward them to DNS.
bool isLoopback(std::string_view host) noexcept {
    return host == "localhost" || host == "127.0.0.1" || host == "[::1]";
}

core::Error invalid(std::string reason) {
    return core::Error::invalidArgument("invalid endpoint: " + std::move(reason));
}

} // namespace

std::string_view toString(Endpoint::Scheme scheme) noexcept {
    switch (scheme) {
    case Endpoint::Scheme::Https:
        return "https";
    case Endpoint::Scheme::Wss:
        return "wss";
    case Endpoint::Scheme::Http:
        return "http";
    case Endpoint::Scheme::Ws:
        return "ws";
    }
    return "https";
}

namespace {

struct HostPort {
    std::string_view host;
    std::string_view port; // empty = default
};

core::Result<void> checkCharacters(std::string_view url) {
    if (url.empty()) {
        return invalid("empty URL");
    }
    if (url.size() > Endpoint::kMaxLength) {
        return invalid("URL longer than 2048 characters");
    }
    const bool hasControl = std::any_of(url.begin(), url.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u <= 0x20 || u == 0x7F;
    });
    if (hasControl) {
        return invalid("URL contains whitespace or control characters");
    }
    return {};
}

core::Result<HostPort> splitIpv6Authority(std::string_view authority) {
    const auto close = authority.find(']');
    if (close == std::string_view::npos || !isIpv6Literal(authority.substr(1, close - 1))) {
        return invalid("malformed IPv6 address");
    }
    HostPort out{authority.substr(0, close + 1), {}};
    const std::string_view after = authority.substr(close + 1);
    if (after.empty()) {
        return out;
    }
    if (after.front() != ':') {
        return invalid("unexpected characters after IPv6 address");
    }
    if (after.size() == 1) {
        return invalid("empty port");
    }
    out.port = after.substr(1);
    return out;
}

core::Result<HostPort> splitAuthority(std::string_view authority) {
    if (authority.find('@') != std::string_view::npos) {
        return invalid("credentials in URLs are not allowed");
    }
    if (!authority.empty() && authority.front() == '[') {
        return splitIpv6Authority(authority);
    }
    const auto colon = authority.rfind(':');
    HostPort out{authority.substr(0, colon), {}};
    if (colon != std::string_view::npos) {
        if (colon + 1 == authority.size()) {
            return invalid("empty port");
        }
        out.port = authority.substr(colon + 1);
    }
    if (!isDnsOrIpv4Host(out.host)) {
        return invalid("missing or malformed host");
    }
    return out;
}

core::Result<std::uint16_t> parsePort(std::string_view text, std::uint16_t fallback) {
    if (text.empty()) {
        return fallback;
    }
    unsigned value = 0;
    const char *first = text.data();
    const char *last =
        first + text.size(); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [end, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || end != last || value == 0 || value > 65535) {
        return invalid("port must be a number between 1 and 65535");
    }
    return static_cast<std::uint16_t>(value);
}

} // namespace

core::Result<Endpoint> Endpoint::parse(std::string_view url) {
    if (auto chars = checkCharacters(url); !chars) {
        return chars.error();
    }
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos) {
        return invalid("missing scheme (expected https://, wss://, http:// or ws://)");
    }
    const auto scheme = parseScheme(url.substr(0, schemeEnd));
    if (!scheme) {
        return invalid("unsupported scheme (expected https, wss, http or ws)");
    }

    const std::string_view rest = url.substr(schemeEnd + 3);
    const auto authorityEnd = rest.find_first_of("/?#");
    auto hostPort = splitAuthority(rest.substr(0, authorityEnd));
    if (!hostPort) {
        return hostPort.error();
    }
    auto port = parsePort(hostPort.value().port, defaultPort(*scheme));
    if (!port) {
        return port.error();
    }

    std::string host = lowered(hostPort.value().host);
    const bool secure = *scheme == Scheme::Https || *scheme == Scheme::Wss;
    if (!secure && !isLoopback(host)) {
        return invalid("plain http/ws is only allowed for localhost; use https or wss");
    }
    std::string target = authorityEnd == std::string_view::npos
                             ? std::string()
                             : std::string(rest.substr(authorityEnd));
    return Endpoint(*scheme, std::move(host), port.value(), std::move(target));
}

bool Endpoint::isSecure() const noexcept {
    return scheme_ == Scheme::Https || scheme_ == Scheme::Wss;
}

std::string Endpoint::origin() const {
    std::string out(toString(scheme_));
    out += "://";
    out += host_;
    if (port_ != defaultPort(scheme_)) {
        out += ':';
        out += std::to_string(port_);
    }
    return out;
}

std::string Endpoint::url() const {
    return origin() + target_;
}

} // namespace fiaisdk::network
