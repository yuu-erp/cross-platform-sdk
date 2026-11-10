#include "core/infrastructure/in_process_event_bus.h"

#include "core/application/logger.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace fiaisdk::core {

InProcessEventBus::InvocationGuard::InvocationGuard(Slot &slot,
                                                    Slot::Invocation &invocation) noexcept
    : slot_(&slot), invocation_(&invocation) {}

InProcessEventBus::InvocationGuard::~InvocationGuard() {
    Handler released;
    {
        const std::scoped_lock lock(slot_->mutex);
        auto **link = &slot_->running;
        while (*link != invocation_) {
            link = &(*link)->next;
        }
        *link = invocation_->next;
        if (!slot_->active && slot_->running == nullptr) {
            released.swap(slot_->handler);
        }
        slot_->idle.notify_all();
    }
    // Captures may destroy other subscriptions: never release them while holding a bus lock.
}

InProcessEventBus::InProcessEventBus(ILogger &logger)
    : state_(std::make_shared<State>()), logger_(&logger) {}

Subscription InProcessEventBus::subscribe(std::string eventName, Handler handler) {
    if (eventName.empty() || !handler) {
        return Subscription{};
    }
    auto slot = std::make_shared<Slot>(std::move(eventName), std::move(handler));

    // Construct the cancellation closure before exposing the handler. If any allocation
    // below fails, the RAII handle rolls back without leaving an unreachable subscription.
    Subscription subscription([weakState = std::weak_ptr<State>(state_), slot]() noexcept {
        cancelSlot(weakState, slot);
    });
    {
        const std::scoped_lock lock(state_->mutex);
        const auto found = state_->topics.find(slot->eventName);
        auto updated = std::make_shared<Slots>();
        if (found != state_->topics.end()) {
            updated->reserve(found->second->size() + 1);
            for (const auto &existing : *found->second) {
                if (existing->active) {
                    updated->push_back(existing);
                }
            }
        }
        updated->push_back(slot);
        if (found == state_->topics.end()) {
            state_->topics.emplace(slot->eventName, std::move(updated));
        } else {
            found->second = std::move(updated);
        }
        slot->registered = true;
        ++state_->activeCount;
    }
    return subscription;
}

void InProcessEventBus::cancelSlot(const std::weak_ptr<State> &weakState,
                                   const std::shared_ptr<Slot> &slot) noexcept {
    Handler released;
    try {
        // Deactivate even if pruning the snapshot runs out of memory.
        {
            std::unique_lock lock(slot->mutex);
            slot->active = false;
            const auto self = std::this_thread::get_id();
            slot->idle.wait(lock, [&] {
                for (auto *call = slot->running; call != nullptr; call = call->next) {
                    if (call->thread != self) {
                        return false;
                    }
                }
                return true;
            });
            if (slot->running == nullptr) {
                released.swap(slot->handler);
            }
        }
        if (const auto state = weakState.lock()) {
            const std::scoped_lock lock(state->mutex);
            if (!slot->registered) {
                return; // subscribe never committed
            }
            slot->registered = false;
            --state->activeCount;
            const auto found = state->topics.find(slot->eventName);
            if (found == state->topics.end()) {
                return; // subscribe never committed
            }
            const auto &slots = *found->second;
            if (std::find(slots.begin(), slots.end(), slot) == slots.end()) {
                return; // subscribe failed or a concurrent subscribe already pruned this slot
            }
            const auto active = std::count_if(
                slots.begin(), slots.end(), [](const auto &entry) { return entry->active.load(); });
            if (active == 0) {
                state->topics.erase(found);
            } else {
                auto updated = std::make_shared<Slots>();
                updated->reserve(static_cast<std::size_t>(active));
                for (const auto &entry : slots) {
                    if (entry->active) {
                        updated->push_back(entry);
                    }
                }
                found->second = std::move(updated);
            }
        }
    } catch (...) { // NOLINT(bugprone-empty-catch)
        // Allocation failure may retain inactive slot metadata until the next topic update.
        // Its handler is still deactivated and its captures released once all calls finish.
    }
}

void InProcessEventBus::invoke(Slot &slot, const DomainEvent &event) noexcept {
    Slot::Invocation invocation{std::this_thread::get_id()};
    try {
        const std::scoped_lock lock(slot.mutex);
        if (!slot.active) {
            return;
        }
        invocation.next = slot.running;
        slot.running = &invocation;
    } catch (...) {
        Logger(*logger_, "core.event_bus").error("could not enter event handler");
        return;
    }
    const InvocationGuard guard(slot, invocation);
    const Logger log(*logger_, "core.event_bus");
    try {
        slot.handler(event);
    } catch (const std::exception &e) {
        try {
            log.error(std::string("handler for '") + std::string(event.name()) +
                      "' threw: " + e.what());
        } catch (...) {
            log.error("event handler threw (details unavailable)");
        }
    } catch (...) {
        log.error("event handler threw a non-standard exception");
    }
}

void InProcessEventBus::publish(const DomainEvent &event) noexcept {
    std::shared_ptr<const Slots> matching;
    try {
        const std::scoped_lock lock(state_->mutex);
        const auto found = state_->topics.find(event.name());
        if (found == state_->topics.end()) {
            return;
        }
        matching = found->second;
    } catch (...) {
        Logger(*logger_, "core.event_bus").error("publish failed: out of memory or lock error");
        return;
    }
    for (const auto &slot : *matching) {
        invoke(*slot, event);
    }
}

std::size_t InProcessEventBus::subscriberCount() const {
    const std::scoped_lock lock(state_->mutex);
    return state_->activeCount;
}

} // namespace fiaisdk::core
