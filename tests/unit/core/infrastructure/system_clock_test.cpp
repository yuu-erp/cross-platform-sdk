#include "core/infrastructure/system_clock.h"

#include <gtest/gtest.h>

#include <chrono>

namespace fiaisdk::core {
namespace {

TEST(SystemClock, NowIsTheSystemWallClock) {
    // Wall clocks may step (NTP), so allow a generous window instead of strict ordering.
    const SystemClock clock;
    const auto difference = clock.now() - std::chrono::system_clock::now();
    EXPECT_LT(std::chrono::abs(difference), std::chrono::seconds(5));
}

TEST(SystemClock, MonotonicNeverGoesBackwards) {
    const SystemClock clock;
    auto previous = clock.monotonicNow();
    for (int i = 0; i < 1000; ++i) {
        const auto current = clock.monotonicNow();
        EXPECT_LE(previous, current);
        previous = current;
    }
}

} // namespace
} // namespace fiaisdk::core
