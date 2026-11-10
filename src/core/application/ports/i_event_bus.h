#pragma once

#include "core/application/subscription.h"
#include "core/domain/domain_event.h"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace fiaisdk::core {

/// Port: in-SDK publish/subscribe of DomainEvents. The documented way for modules to react to
/// each other without depending on each other's internals.
///
/// Delivery contract (all implementations):
/// - publish() calls matching handlers synchronously on the publishing thread, in subscription
///   order, and never throws: a throwing handler is logged and the others still run.
/// - subscribe/publish/cancel are thread-safe across different Subscription handles. A single
///   Subscription handle requires external synchronization if accessed by multiple threads.
/// - A publish uses a subscription snapshot: newly subscribed handlers join the next publish
///   (including a nested publish); handlers cancelled before invocation are skipped.
/// - The event is borrowed only for the duration of publish(). Handlers must copy data they
///   retain and must tolerate concurrent invocations by different publishing threads.
/// - When Subscription::cancel() (or its destructor) returns, the handler is not running on any
///   other thread and will not be called again, so the objects it captures may be destroyed.
///   Cancelling from inside the same handler does not wait for itself.
/// - Do not cancel subscription B from inside handler A while B may be running and B may cancel
///   A at the same time: both cancels wait for each other (deadlock).
/// - Handlers must not block for long; hand heavy work to the module's own executor (async ADR).
class IEventBus {
  public:
    using Handler = std::function<void(const DomainEvent &)>;

    IEventBus() = default;
    virtual ~IEventBus() = default;
    IEventBus(const IEventBus &) = delete;
    IEventBus &operator=(const IEventBus &) = delete;
    IEventBus(IEventBus &&) = delete;
    IEventBus &operator=(IEventBus &&) = delete;

    /// Handler runs for every published event whose name() equals `eventName`. The returned
    /// Subscription unsubscribes when destroyed. An empty name or handler yields an inactive
    /// Subscription.
    [[nodiscard]] virtual Subscription subscribe(std::string eventName, Handler handler) = 0;

    virtual void publish(const DomainEvent &event) noexcept = 0;
};

/// Type-safe subscription: `auto sub = subscribeTo<WalletCreated>(bus, [](const WalletCreated& e)
/// {});` Event must derive from DomainEvent and define `static constexpr std::string_view kName`.
template <class Event, class Fn> [[nodiscard]] Subscription subscribeTo(IEventBus &bus, Fn &&fn) {
    static_assert(std::is_base_of_v<DomainEvent, Event>, "Event must derive from DomainEvent");
    return bus.subscribe(std::string(Event::kName),
                         [handler = std::forward<Fn>(fn)](const DomainEvent &event) {
                             if (const auto *typed = dynamic_cast<const Event *>(&event)) {
                                 handler(*typed);
                             }
                         });
}

} // namespace fiaisdk::core
