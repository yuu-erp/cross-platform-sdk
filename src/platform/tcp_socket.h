#pragma once

#include "core/domain/result.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace fiaisdk::platform {

/// Owns one connected TCP socket. Destruction closes it.
class TcpSocket {
  public:
    virtual ~TcpSocket() = default;
    /// Known peer closure, without waiting or consuming data.
    [[nodiscard]] virtual bool peerClosed() const noexcept = 0;
};

struct TcpConnection {
    std::chrono::milliseconds latency{0};
    std::unique_ptr<TcpSocket> socket;
};

/// host is an unbracketed numeric IPv4 or IPv6 address.
[[nodiscard]] core::Result<TcpConnection> openTcpSocket(const std::string &host, std::uint16_t port,
                                                        std::chrono::milliseconds timeout);

} // namespace fiaisdk::platform
