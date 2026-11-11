// The POSIX implementation is shared by Linux and macOS.
#include "platform/tcp_socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <memory>

namespace fiaisdk::platform {
namespace {

using Clock = std::chrono::steady_clock;

class Socket final : public TcpSocket {
  public:
    explicit Socket(int family) noexcept : fd(::socket(family, SOCK_STREAM, IPPROTO_TCP)) {}
    ~Socket() override {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&) = delete;
    Socket &operator=(Socket &&) = delete;

    [[nodiscard]] bool peerClosed() const noexcept override {
        pollfd descriptor{.fd = fd, .events = POLLIN, .revents = 0};
        if (::poll(&descriptor, 1, 0) <= 0) {
            return false;
        }
        if ((static_cast<unsigned>(descriptor.revents) &
             static_cast<unsigned>(POLLHUP | POLLERR | POLLNVAL)) != 0) {
            return true;
        }
        char byte = 0;
        const auto received = ::recv(fd, &byte, sizeof(byte), MSG_PEEK);
        return received == 0 ||
               (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR);
    }

    const int fd;
};

bool waitForConnect(int fd, Clock::time_point deadline) noexcept {
    pollfd descriptor{.fd = fd, .events = POLLOUT, .revents = 0};
    for (auto now = Clock::now(); now < deadline; now = Clock::now()) {
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
        const int ready = ::poll(&descriptor, 1, static_cast<int>(remaining.count()));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        int error = 0;
        socklen_t length = sizeof(error);
        return ready > 0 && ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
               error == 0;
    }
    return false;
}

} // namespace

core::Result<TcpConnection> openTcpSocket(const std::string &host, std::uint16_t port,
                                          std::chrono::milliseconds timeout) {
    const auto start = Clock::now();
    sockaddr_in ipv4{};
    sockaddr_in6 ipv6{};
    const sockaddr *address = nullptr;
    socklen_t length = 0;
    int family = AF_INET;
    if (::inet_pton(AF_INET, host.c_str(), &ipv4.sin_addr) == 1) {
        ipv4.sin_family = AF_INET;
        ipv4.sin_port = htons(port);
        // The socket API takes sockaddr for both IPv4 and IPv6.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        address = reinterpret_cast<const sockaddr *>(&ipv4);
        length = sizeof(ipv4);
    } else if (::inet_pton(AF_INET6, host.c_str(), &ipv6.sin6_addr) == 1) {
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
    const int fd = socket->fd;
    if (fd < 0 || ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 || ::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        return core::Error::unavailable("could not initialize TCP socket");
    }
    if (::connect(fd, address, length) < 0 &&
        ((errno != EINPROGRESS && errno != EINTR) || !waitForConnect(fd, start + timeout))) {
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
