#include "modules/network/infrastructure/simulated_node_probe.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace fiaisdk::network {

std::chrono::milliseconds SimulatedNodeProbe::simulatedLatency(const std::string &host) noexcept {
    // FNV-1a: stable across platforms and runs (unlike std::hash).
    std::uint32_t hash = 2166136261U;
    for (const char c : host) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 16777619U;
    }
    constexpr std::uint32_t kMinMs = 5;
    constexpr std::uint32_t kSpreadMs = 45;
    return std::chrono::milliseconds(kMinMs + (hash % kSpreadMs));
}

core::Result<ProbeResult> SimulatedNodeProbe::probe(const Endpoint &endpoint,
                                                    std::chrono::milliseconds timeout) {
    constexpr std::string_view kUnreachableSuffix = ".invalid";
    const std::string &host = endpoint.host();
    if (host.size() >= kUnreachableSuffix.size() &&
        std::string_view(host).substr(host.size() - kUnreachableSuffix.size()) ==
            kUnreachableSuffix) {
        return core::Error::unavailable("node unreachable: " + endpoint.origin() + " (simulated)");
    }
    const auto latency = simulatedLatency(host);
    if (latency > timeout) {
        return core::Error::unavailable("node did not answer within " +
                                        std::to_string(timeout.count()) +
                                        " ms: " + endpoint.origin() + " (simulated)");
    }
    return ProbeResult{latency};
}

} // namespace fiaisdk::network
