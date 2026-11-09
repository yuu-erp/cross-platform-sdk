#include "modules/network/infrastructure/simulated_node_probe.h"

#include <gtest/gtest.h>

#include <chrono>

namespace fiaisdk::network {
namespace {

using namespace std::chrono_literals;

Endpoint endpoint(const char *url) {
    return Endpoint::parse(url).value();
}

TEST(SimulatedNodeProbe, ReservedInvalidTldIsUnreachable) {
    SimulatedNodeProbe probe;
    const auto result = probe.probe(endpoint("https://down.invalid"), 5s);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::Unavailable);
}

TEST(SimulatedNodeProbe, LatencyIsDeterministicAndInRange) {
    SimulatedNodeProbe probe;
    const auto first = probe.probe(endpoint("https://node.example.com"), 5s);
    const auto second = probe.probe(endpoint("https://node.example.com/other"), 5s);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.value().latency, second.value().latency); // depends on host only
    EXPECT_GE(first.value().latency, 5ms);
    EXPECT_LE(first.value().latency, 49ms);
}

TEST(SimulatedNodeProbe, LatencyAboveTimeoutIsUnavailable) {
    SimulatedNodeProbe probe;
    const auto latency = SimulatedNodeProbe::simulatedLatency("node.example.com");
    EXPECT_TRUE(probe.probe(endpoint("https://node.example.com"), latency));
    const auto tooShort = probe.probe(endpoint("https://node.example.com"), latency - 1ms);
    ASSERT_FALSE(tooShort);
    EXPECT_EQ(tooShort.error().code, core::ErrorCode::Unavailable);
}

} // namespace
} // namespace fiaisdk::network
