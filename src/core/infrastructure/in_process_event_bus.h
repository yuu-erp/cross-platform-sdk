#pragma once

#include "core/application/ports/i_event_bus.h"
#include "core/application/ports/i_logger.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
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
        bool active = true;
        std::vector<std::thread::id> running; // threads currently inside handler
        Handler handler;                      // reset after cancel to release captures
    };
    struct State {
        mutable std::mutex mutex;
        std::uint64_t nextId = 1;
        std::map<std::uint64_t, std::shared_ptr<Slot>> slots; // ordered = subscription order
    };

    static void cancelSlot(const std::weak_ptr<State> &state, std::uint64_t id,
                           const std::shared_ptr<Slot> &slot) noexcept;
    void invoke(Slot &slot, const DomainEvent &event) noexcept;

    std::shared_ptr<State> state_;
    ILogger *logger_;
};

} // namespace fiaisdk::core
