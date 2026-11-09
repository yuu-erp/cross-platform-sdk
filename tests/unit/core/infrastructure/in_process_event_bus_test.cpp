#include "core/infrastructure/in_process_event_bus.h"
#include "support/core/fake_clock.h"
#include "support/core/recording_logger.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace fiaisdk::core {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

class WalletCreated final : public DomainEvent {
  public:
    static constexpr std::string_view kName = "wallet.created";
    WalletCreated(Timestamp at, std::string walletId)
        : DomainEvent(at), walletId_(std::move(walletId)) {}
    [[nodiscard]] std::string_view name() const noexcept override {
        return kName;
    }
    [[nodiscard]] const std::string &walletId() const {
        return walletId_;
    }

  private:
    std::string walletId_;
};

/// Same name as WalletCreated but a different type: typed subscribers must ignore it.
class ImpostorEvent final : public DomainEvent {
  public:
    explicit ImpostorEvent(Timestamp at) : DomainEvent(at) {}
    [[nodiscard]] std::string_view name() const noexcept override {
        return WalletCreated::kName;
    }
};

class OtherEvent final : public DomainEvent {
  public:
    explicit OtherEvent(Timestamp at) : DomainEvent(at) {}
    [[nodiscard]] std::string_view name() const noexcept override {
        return "network.connected";
    }
};

class InProcessEventBusTest : public ::testing::Test {
  protected:
    test::FakeClock clock_;
    test::RecordingLogger logger_;
    InProcessEventBus bus_{logger_};
};

TEST_F(InProcessEventBusTest, DeliversOnlyMatchingEventsInSubscriptionOrder) {
    std::vector<std::string> calls;
    auto first =
        bus_.subscribe("wallet.created", [&](const DomainEvent &) { calls.push_back("1"); });
    auto second =
        bus_.subscribe("wallet.created", [&](const DomainEvent &) { calls.push_back("2"); });
    auto other =
        bus_.subscribe("network.connected", [&](const DomainEvent &) { calls.push_back("x"); });

    bus_.publish(WalletCreated(clock_.now(), "w1"));
    EXPECT_THAT(calls, ElementsAre("1", "2"));
}

TEST_F(InProcessEventBusTest, TypedSubscriptionReceivesConcreteEvent) {
    std::string received;
    auto subscription = subscribeTo<WalletCreated>(
        bus_, [&](const WalletCreated &event) { received = event.walletId(); });

    bus_.publish(ImpostorEvent(clock_.now())); // same name, wrong type: ignored
    EXPECT_TRUE(received.empty());
    bus_.publish(WalletCreated(clock_.now(), "w42"));
    EXPECT_EQ(received, "w42");
}

TEST_F(InProcessEventBusTest, EventCarriesTimestampFromTheClock) {
    Timestamp seen{};
    auto subscription = bus_.subscribe(
        "wallet.created", [&](const DomainEvent &event) { seen = event.occurredAt(); });
    clock_.advance(std::chrono::seconds(5));
    bus_.publish(WalletCreated(clock_.now(), "w"));
    EXPECT_EQ(seen, clock_.now());
}

TEST_F(InProcessEventBusTest, CancelledSubscriptionStopsDelivery) {
    int calls = 0;
    auto subscription = bus_.subscribe("wallet.created", [&](const DomainEvent &) { ++calls; });
    EXPECT_EQ(bus_.subscriberCount(), 1U);
    subscription.cancel();
    EXPECT_EQ(bus_.subscriberCount(), 0U);
    bus_.publish(WalletCreated(clock_.now(), "w"));
    EXPECT_EQ(calls, 0);
}

TEST_F(InProcessEventBusTest, EmptyNameOrHandlerGivesInactiveSubscription) {
    EXPECT_FALSE(bus_.subscribe("", [](const DomainEvent &) {}).active());
    EXPECT_FALSE(bus_.subscribe("wallet.created", nullptr).active());
    EXPECT_EQ(bus_.subscriberCount(), 0U);
}

TEST_F(InProcessEventBusTest, ThrowingHandlerIsLoggedAndOthersStillRun) {
    int laterCalls = 0;
    auto bad = bus_.subscribe("wallet.created",
                              [](const DomainEvent &) { throw std::runtime_error("kaboom"); });
    auto good = bus_.subscribe("wallet.created", [&](const DomainEvent &) { ++laterCalls; });

    bus_.publish(WalletCreated(clock_.now(), "w"));

    EXPECT_EQ(laterCalls, 1);
    const auto entries = logger_.entries();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].level, LogLevel::Error);
    EXPECT_EQ(entries[0].category, "core.event_bus");
    EXPECT_THAT(entries[0].message, HasSubstr("kaboom"));
}

TEST_F(InProcessEventBusTest, HandlersMayPublishAndUnsubscribeReentrantly) {
    int otherCalls = 0;
    auto other = bus_.subscribe("network.connected", [&](const DomainEvent &) { ++otherCalls; });
    Subscription self;
    self = bus_.subscribe("wallet.created", [&](const DomainEvent &) {
        bus_.publish(OtherEvent(clock_.now())); // nested publish: no deadlock
        self.cancel();                          // unsubscribe from inside the handler
    });

    bus_.publish(WalletCreated(clock_.now(), "w"));
    bus_.publish(WalletCreated(clock_.now(), "w"));
    EXPECT_EQ(otherCalls, 1);
}

TEST_F(InProcessEventBusTest, CancelWaitsForHandlerRunningOnAnotherThread) {
    std::promise<void> started;
    std::atomic<bool> finished{false};
    auto subscription = bus_.subscribe("wallet.created", [&](const DomainEvent &) {
        started.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        finished = true;
    });

    std::thread publisher([this] { bus_.publish(WalletCreated(clock_.now(), "w")); });
    started.get_future().wait();
    subscription.cancel(); // must block until the handler above returns
    EXPECT_TRUE(finished.load());
    publisher.join();
}

TEST_F(InProcessEventBusTest, CancelReleasesTheHandlerCaptures) {
    auto owned = std::make_shared<int>(1);
    const std::weak_ptr<int> watch = owned;
    auto subscription = bus_.subscribe("wallet.created", [owned](const DomainEvent &) {});
    owned.reset();
    EXPECT_FALSE(watch.expired());
    subscription.cancel();
    EXPECT_TRUE(watch.expired());
}

TEST(InProcessEventBus, SubscriptionMayOutliveTheBus) {
    test::RecordingLogger logger;
    Subscription subscription;
    {
        InProcessEventBus bus(logger);
        subscription = bus.subscribe("wallet.created", [](const DomainEvent &) {});
    }
    subscription.cancel(); // bus is gone: must be a safe no-op
    EXPECT_FALSE(subscription.active());
}

TEST_F(InProcessEventBusTest, ConcurrentSubscribePublishCancelIsSafe) {
    std::atomic<int> delivered{0};
    auto keep = bus_.subscribe("wallet.created", [&](const DomainEvent &) { ++delivered; });

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this] {
            for (int i = 0; i < 200; ++i) {
                auto temporary = bus_.subscribe("wallet.created", [](const DomainEvent &) {});
                bus_.publish(WalletCreated(clock_.now(), "w"));
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(delivered.load(), 4 * 200);
    EXPECT_EQ(bus_.subscriberCount(), 1U);
}

} // namespace
} // namespace fiaisdk::core
