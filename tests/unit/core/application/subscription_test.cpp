#include "core/application/subscription.h"

#include <gtest/gtest.h>

#include <utility>

namespace fiaisdk::core {
namespace {

TEST(Subscription, DefaultIsInactive) {
    const Subscription subscription;
    EXPECT_FALSE(subscription.active());
}

TEST(Subscription, CancelsExactlyOnceOnDestruction) {
    int cancelled = 0;
    {
        Subscription subscription([&cancelled]() noexcept { ++cancelled; });
        EXPECT_TRUE(subscription.active());
    }
    EXPECT_EQ(cancelled, 1);
}

TEST(Subscription, ExplicitCancelIsIdempotent) {
    int cancelled = 0;
    Subscription subscription([&cancelled]() noexcept { ++cancelled; });
    subscription.cancel();
    subscription.cancel();
    EXPECT_FALSE(subscription.active());
    EXPECT_EQ(cancelled, 1);
}

TEST(Subscription, MoveTransfersOwnership) {
    int cancelled = 0;
    Subscription first([&cancelled]() noexcept { ++cancelled; });
    Subscription second(std::move(first));
    EXPECT_FALSE(first.active()); // NOLINT(bugprone-use-after-move): verifying moved-from state
    EXPECT_TRUE(second.active());

    Subscription third;
    third = std::move(second);
    EXPECT_EQ(cancelled, 0);
    third.cancel();
    EXPECT_EQ(cancelled, 1);
}

TEST(Subscription, MoveAssignCancelsThePreviousOne) {
    int first = 0;
    int second = 0;
    Subscription target([&first]() noexcept { ++first; });
    target = Subscription([&second]() noexcept { ++second; });
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);
}

} // namespace
} // namespace fiaisdk::core
