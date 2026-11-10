#pragma once

#include "core/application/ports/i_event_bus.h"
#include "core/application/ports/i_logger.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fiaisdk::core {

/// Synchronous, thread-safe IEventBus for one SDK instance (see IEventBus for the contract).
///
/// Each subscription is a Slot. Cancelling marks the slot inactive and then waits until no other
/// thread is still running its handler, so after Subscription::cancel() returns the captured
/// object may be destroyed safely. Subscriptions keep only a weak reference to the bus, so a
/// Subscription may outlive it. `logger` must outlive the bus; it reports handlers that throw.
/// Topic snapshots are copied on subscription changes. Publishing does no bus-owned allocation
/// and visits only the matching topic. This favours frequent events and stable subscriptions.
class InProcessEventBus final : public IEventBus {
  public:
    explicit InProcessEventBus(ILogger &logger);

    [[nodiscard]] Subscription subscribe(std::string eventName, Handler handler) override;
    void publish(const DomainEvent &event) noexcept override;

    /// Number of active subscriptions (diagnostics and tests).
    [[nodiscard]] std::size_t subscriberCount() const;

  private:
    struct Slot {
        Slot(std::string name, Handler fn) : eventName(std::move(name)), handler(std::move(fn)) {}

        const std::string eventName;
        std::mutex mutex;
        std::condition_variable idle;
        std::atomic<bool> active{true};
        struct Invocation {
            std::thread::id thread;
            Invocation *next = nullptr;
        };
        Invocation *running = nullptr; // stack frames linked while holding mutex
        Handler handler;               // released outside locks once inactive and idle
        bool registered = false;       // protected by State::mutex
    };
    using Slots = std::vector<std::shared_ptr<Slot>>;
    struct State {
        mutable std::mutex mutex;
        // Immutable snapshots: publishing only copies a shared_ptr, never the vector.
        std::map<std::string, std::shared_ptr<const Slots>, std::less<>> topics;
        std::size_t activeCount = 0;
    };

    class InvocationGuard {
      public:
        InvocationGuard(Slot &slot, Slot::Invocation &invocation) noexcept;
        ~InvocationGuard();
        InvocationGuard(const InvocationGuard &) = delete;
        InvocationGuard &operator=(const InvocationGuard &) = delete;

      private:
        Slot *slot_;
        Slot::Invocation *invocation_;
    };

    static void cancelSlot(const std::weak_ptr<State> &state,
                           const std::shared_ptr<Slot> &slot) noexcept;
    void invoke(Slot &slot, const DomainEvent &event) noexcept;

    std::shared_ptr<State> state_;
    ILogger *logger_;
};

} // namespace fiaisdk::core
