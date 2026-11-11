#include "modules/network/infrastructure/tcp_node_connector.h"

#include "platform/tcp_socket.h"

#include <memory>
#include <utility>

namespace fiaisdk::network {

namespace {

class TcpNodeConnection final : public NodeConnection {
  public:
    explicit TcpNodeConnection(std::unique_ptr<platform::TcpSocket> socket) noexcept
        : socket_(std::move(socket)) {}

    [[nodiscard]] bool peerClosed() const noexcept override {
        return socket_->peerClosed();
    }

  private:
    std::unique_ptr<platform::TcpSocket> socket_;
};

} // namespace

core::Result<OpenedConnection> TcpNodeConnector::open(const Endpoint &endpoint,
                                                      std::chrono::milliseconds timeout) {
    if (endpoint.scheme() != Endpoint::Scheme::Tcp) {
        return core::Error::invalidArgument("the TCP adapter requires a tcp:// endpoint");
    }
    const auto &host = endpoint.host();
    auto opened = platform::openTcpSocket(
        host.front() == '[' ? host.substr(1, host.size() - 2) : host, endpoint.port(), timeout);
    if (!opened) {
        return opened.error();
    }
    auto &connection = opened.value();
    return OpenedConnection{
        .latency = connection.latency,
        .connection = std::make_unique<TcpNodeConnection>(std::move(connection.socket)),
    };
}

} // namespace fiaisdk::network
