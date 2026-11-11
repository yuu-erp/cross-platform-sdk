// The network C++ wrapper, used as a consumer would (built as C++17).
#include <fiaisdk/cpp/network.hpp>

#include "network/loopback_listener.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

class Listener {
  public:
    explicit Listener(bool listening = true) : value(test_open_loopback(listening ? 1 : 0)) {}
    ~Listener() {
        test_close_loopback(value);
    }
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    const loopback_listener value;
};

class Peer {
  public:
    explicit Peer(test_socket accepted) : socket(accepted) {}
    ~Peer() {
        if (socket != TEST_BAD_SOCKET) {
            test_close_socket(socket);
        }
    }
    Peer(const Peer &) = delete;
    Peer &operator=(const Peer &) = delete;

    void expectClosed() const {
        const int readable = test_socket_readable(socket, 2);
        ASSERT_EQ(readable, 1);
        char byte = 0;
        EXPECT_EQ(recv(socket, &byte, 1, 0), 0);
    }

    const test_socket socket;
};

TEST(NetworkWrapper, ConnectStatusEndpointDisconnect) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);

    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Disconnected);
    EXPECT_EQ(network.endpoint(), "");

    const Listener listener;
    ASSERT_NE(listener.value.socket, TEST_BAD_SOCKET);
    network.connect(listener.value.url, 2s);
    ASSERT_EQ(test_socket_readable(listener.value.socket, 2), 1);
    const Peer peer(accept(listener.value.socket, nullptr, nullptr));
    ASSERT_NE(peer.socket, TEST_BAD_SOCKET);
    EXPECT_EQ(test_socket_readable(peer.socket, 0), 0);
    network.connect(listener.value.url, 2s);
    EXPECT_EQ(test_socket_readable(listener.value.socket, 0), 0);

    const Listener closedPort(false);
    ASSERT_NE(closedPort.value.socket, TEST_BAD_SOCKET);
    EXPECT_THROW(network.connect(closedPort.value.url, 2s), fiaisdk::Error);
    EXPECT_EQ(test_socket_readable(peer.socket, 0), 0);
    const auto status = network.status();
    EXPECT_EQ(status.state, fiaisdk::NetworkState::Connected);
    EXPECT_LE(status.latency, 2s);
    EXPECT_GT(status.connectedAt.time_since_epoch().count(), 0);
    EXPECT_EQ(network.endpoint(), listener.value.url);

    network.disconnect();
    peer.expectClosed();
    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Disconnected);
}

TEST(NetworkWrapper, SwitchAndSdkDestructionCloseSockets) {
    const Listener first;
    const Listener second;
    ASSERT_NE(first.value.socket, TEST_BAD_SOCKET);
    ASSERT_NE(second.value.socket, TEST_BAD_SOCKET);

    auto sdk = std::make_unique<fiaisdk::Sdk>();
    const fiaisdk::Network network(*sdk);
    network.connect(first.value.url, 2s);
    ASSERT_EQ(test_socket_readable(first.value.socket, 2), 1);
    const Peer previous(accept(first.value.socket, nullptr, nullptr));
    ASSERT_NE(previous.socket, TEST_BAD_SOCKET);

    network.connect(second.value.url, 2s);
    previous.expectClosed();
    ASSERT_EQ(test_socket_readable(second.value.socket, 2), 1);
    const Peer current(accept(second.value.socket, nullptr, nullptr));
    ASSERT_NE(current.socket, TEST_BAD_SOCKET);
    EXPECT_EQ(test_socket_readable(current.socket, 0), 0);

    sdk.reset();
    current.expectClosed();
}

TEST(NetworkWrapper, StatusDetectsRemoteClose) {
    const Listener listener;
    ASSERT_NE(listener.value.socket, TEST_BAD_SOCKET);
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);
    network.connect(listener.value.url, 2s);
    ASSERT_EQ(test_socket_readable(listener.value.socket, 2), 1);
    {
        const Peer peer(accept(listener.value.socket, nullptr, nullptr));
        ASSERT_NE(peer.socket, TEST_BAD_SOCKET);
        ASSERT_EQ(network.status().state, fiaisdk::NetworkState::Connected);
    }

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (network.status().state == fiaisdk::NetworkState::Connected &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }
    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Disconnected);
    EXPECT_TRUE(network.endpoint().empty());
    network.connect(listener.value.url, 2s);
    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Connected);
}

TEST(NetworkWrapper, ErrorsCarryTheStatusCode) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);
    const Listener closedPort(false);
    ASSERT_NE(closedPort.value.socket, TEST_BAD_SOCKET);
    try {
        network.connect(closedPort.value.url);
        FAIL() << "expected fiaisdk::Error";
    } catch (const fiaisdk::Error &e) {
        EXPECT_EQ(e.status(), FIAISDK_STATUS_UNAVAILABLE);
    }
    try {
        network.connect("http://node.example.com");
        FAIL() << "expected fiaisdk::Error";
    } catch (const fiaisdk::Error &e) {
        EXPECT_EQ(e.status(), FIAISDK_STATUS_INVALID_ARGUMENT);
    }
    EXPECT_THROW(network.connect("https://node.example.com", 0ms), fiaisdk::Error);
}

TEST(NetworkWrapper, ConcurrentConnectsAndReadsAreSafe) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);
    const Listener listener;
    ASSERT_NE(listener.value.socket, TEST_BAD_SOCKET);
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 100; ++i) {
                try {
                    network.connect(listener.value.url, 1s);
                    static_cast<void>(network.status());
                    static_cast<void>(network.endpoint());
                } catch (...) {
                    ++failures;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Connected);
}

} // namespace
