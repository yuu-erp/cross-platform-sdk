#include "platform/tcp_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <memory>

namespace fiaisdk::platform {
namespace {

using Clock = std::chrono::steady_clock;

class Socket final : public TcpSocket {
  public:
    explicit Socket(int family) noexcept {
        WSADATA data{};
        started_ = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
        if (started_) {
            handle = ::WSASocketW(family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                  WSA_FLAG_NO_HANDLE_INHERIT);
        }
    }
    ~Socket() override {
        if (handle != INVALID_SOCKET) {
            ::closesocket(handle);
        }
        if (started_) {
            ::WSACleanup();
        }
    }
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&) = delete;
    Socket &operator=(Socket &&) = delete;

    [[nodiscard]] bool peerClosed() const noexcept override {
        char byte = 0;
        const int received = ::recv(handle, &byte, 1, MSG_PEEK);
        if (received != SOCKET_ERROR) {
            return received == 0;
        }
        const int error = ::WSAGetLastError();
        return error != WSAEWOULDBLOCK && error != WSAEINTR;
    }

    SOCKET handle = INVALID_SOCKET;

  private:
    bool started_ = false;
};

bool waitForConnect(SOCKET socket, Clock::time_point deadline) noexcept {
    const auto remaining = std::chrono::ceil<std::chrono::microseconds>(deadline - Clock::now());
    if (remaining.count() <= 0) {
        return false;
    }
    constexpr long microsecondsPerSecond = 1'000'000;
    timeval timeout{static_cast<long>(remaining.count() / microsecondsPerSecond),
                    static_cast<long>(remaining.count() % microsecondsPerSecond)};
    fd_set writes{};
    fd_set errors{};
    FD_SET(socket, &writes);
    FD_SET(socket, &errors);
    if (::select(0, nullptr, &writes, &errors, &timeout) <= 0) {
        return false;
    }
    int error = 0;
    int length = sizeof(error);
    return ::getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length) ==
               0 &&
           error == 0 && FD_ISSET(socket, &writes);
}

} // namespace

core::Result<TcpConnection> openTcpSocket(const std::string &host, std::uint16_t port,
                                          std::chrono::milliseconds timeout) {
    const auto start = Clock::now();
    sockaddr_in ipv4{};
    sockaddr_in6 ipv6{};
    const sockaddr *address = nullptr;
    int length = 0;
    int family = AF_INET;
    if (::InetPtonA(AF_INET, host.c_str(), &ipv4.sin_addr) == 1) {
        ipv4.sin_family = AF_INET;
        ipv4.sin_port = htons(port);
        // The socket API takes sockaddr for both IPv4 and IPv6.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        address = reinterpret_cast<const sockaddr *>(&ipv4);
        length = sizeof(ipv4);
    } else if (::InetPtonA(AF_INET6, host.c_str(), &ipv6.sin6_addr) == 1) {
        family = AF_INET6;
        ipv6.sin6_family = AF_INET6;
        ipv6.sin6_port = htons(port);
        // The socket API takes sockaddr for both IPv4 and IPv6.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        address = reinterpret_cast<const sockaddr *>(&ipv6);
        length = sizeof(ipv6);
    } else {
        return core::Error::invalidArgument("TCP requires a numeric IPv4 or IPv6 address");
    }

    auto socket = std::make_unique<Socket>(family);
    u_long nonblocking = 1;
    if (socket->handle == INVALID_SOCKET ||
        ::ioctlsocket(socket->handle, FIONBIO, &nonblocking) != 0) {
        return core::Error::unavailable("could not initialize TCP socket");
    }
    if (::connect(socket->handle, address, length) == SOCKET_ERROR &&
        (::WSAGetLastError() != WSAEWOULDBLOCK ||
         !waitForConnect(socket->handle, start + timeout))) {
        return core::Error::unavailable("TCP connection failed or timed out");
    }
    if (Clock::now() >= start + timeout) {
        return core::Error::unavailable("TCP connection timed out");
    }
    return TcpConnection{
        .latency = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start),
        .socket = std::move(socket),
    };
}

} // namespace fiaisdk::platform
