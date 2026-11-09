// The network C++ wrapper, used as a consumer would (built as C++17).
#include <fiaisdk/cpp/network.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

TEST(NetworkWrapper, ConnectStatusEndpointDisconnect) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);

    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Disconnected);
    EXPECT_EQ(network.endpoint(), "");

    network.connect("https://node.example.com/rpc", 2s);
    const auto status = network.status();
    EXPECT_EQ(status.state, fiaisdk::NetworkState::Connected);
    EXPECT_GE(status.latency, 5ms);
    EXPECT_GT(status.connectedAt.time_since_epoch().count(), 0);
    EXPECT_EQ(network.endpoint(), "https://node.example.com/rpc");

    network.disconnect();
    EXPECT_EQ(network.status().state, fiaisdk::NetworkState::Disconnected);
}

TEST(NetworkWrapper, ErrorsCarryTheStatusCode) {
    const fiaisdk::Sdk sdk;
    const fiaisdk::Network network(sdk);
    try {
        network.connect("https://down.invalid");
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
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            const std::string url = "https://node" + std::to_string(t % 2) + ".example.com";
            for (int i = 0; i < 100; ++i) {
                try {
                    network.connect(url, 1s);
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
