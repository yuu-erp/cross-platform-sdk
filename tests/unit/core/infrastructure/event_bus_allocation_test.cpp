#include "core/infrastructure/in_process_event_bus.h"
#include "core/infrastructure/null_logger.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <new>
#include <string_view>

namespace {

// Fault injection is local to this thread and disabled during GoogleTest assertions.
thread_local int allocationBudget = -1;

class AllocationLimit {
  public:
    explicit AllocationLimit(int budget) {
        allocationBudget = budget;
    }
    ~AllocationLimit() {
        allocationBudget = -1;
    }
    AllocationLimit(const AllocationLimit &) = delete;
    AllocationLimit &operator=(const AllocationLimit &) = delete;
};

class Event final : public fiaisdk::core::DomainEvent {
  public:
    Event() : DomainEvent({}) {}
    std::string_view name() const noexcept override {
        return "test.event";
    }
};

} // namespace

// This executable isolates the replacement allocator from all other SDK tests.
void *operator new(std::size_t size) {
    if (allocationBudget == 0) {
        throw std::bad_alloc();
    }
    if (allocationBudget > 0) {
        --allocationBudget;
    }
    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *memory) noexcept {
    std::free(memory);
}
void operator delete[](void *memory) noexcept {
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept {
    return ::operator new(size, tag);
}
void operator delete(void *memory, const std::nothrow_t &) noexcept {
    std::free(memory);
}
void operator delete[](void *memory, const std::nothrow_t &) noexcept {
    std::free(memory);
}

namespace fiaisdk::core {
namespace {

TEST(EventBusAllocation, PublishAndNestedPublishNeedNoAllocation) {
    NullLogger logger;
    InProcessEventBus bus(logger);
    int calls = 0;
    auto subscription = bus.subscribe("test.event", [&](const DomainEvent &event) {
        if (++calls == 1) {
            bus.publish(event);
        }
    });
    {
        const AllocationLimit limit(0);
        bus.publish(Event{});
    }
    EXPECT_EQ(calls, 2);
}

TEST(EventBusAllocation, FailedSubscribeLeavesNoHandlerOrCaptures) {
    // Exercise every allocation up to a successful subscribe, with and without an existing topic.
    for (const bool existingTopic : {false, true}) {
        bool succeeded = false;
        for (int budget = 0; budget < 32 && !succeeded; ++budget) {
            NullLogger logger;
            InProcessEventBus bus(logger);
            Subscription keep;
            if (existingTopic) {
                keep = bus.subscribe("test.event", [](const DomainEvent &) {});
            }
            int calls = 0;
            auto capture = std::make_shared<int>(0);
            const std::weak_ptr<int> watch = capture;
            Subscription subscription;
            try {
                const AllocationLimit limit(budget);
                subscription = bus.subscribe("test.event",
                                             [capture, &calls](const DomainEvent &) { ++calls; });
                succeeded = true;
            } catch (const std::bad_alloc &) {
                // Assertions below run with fault injection disabled.
            }
            capture.reset();
            EXPECT_EQ(bus.subscriberCount(), static_cast<std::size_t>(existingTopic) +
                                                 static_cast<std::size_t>(succeeded));
            bus.publish(Event{});
            EXPECT_EQ(calls, succeeded ? 1 : 0);
            subscription.cancel();
            EXPECT_TRUE(watch.expired());
        }
        EXPECT_TRUE(succeeded);
    }
}

TEST(EventBusAllocation, CancelStillReleasesCapturesWhenSnapshotPruningFails) {
    NullLogger logger;
    InProcessEventBus bus(logger);
    int calls = 0;
    auto keep = bus.subscribe("test.event", [&](const DomainEvent &) { ++calls; });
    auto capture = std::make_shared<int>(0);
    const std::weak_ptr<int> watch = capture;
    auto cancelled = bus.subscribe("test.event", [capture](const DomainEvent &) {});
    capture.reset();
    {
        const AllocationLimit limit(0);
        cancelled.cancel();
        bus.publish(Event{});
    }
    EXPECT_TRUE(watch.expired());
    EXPECT_EQ(bus.subscriberCount(), 1U);
    EXPECT_EQ(calls, 1);
    auto next = bus.subscribe("test.event", [&](const DomainEvent &) { ++calls; });
    EXPECT_EQ(bus.subscriberCount(), 2U);
    bus.publish(Event{});
    EXPECT_EQ(calls, 3);
    keep.cancel();
    next.cancel();
    EXPECT_EQ(bus.subscriberCount(), 0U);
}

} // namespace
} // namespace fiaisdk::core
