#include "core/infrastructure/in_process_event_bus.h"

#include "core/application/logger.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace fiaisdk::core {

namespace {

/// Removes the current thread from Slot::running when the handler returns or throws.
class RunningGuard {
  public:
    RunningGuard(std::mutex &mutex, std::condition_variable &idle,
                 std::vector<std::thread::id> &running) noexcept
        : mutex_(&mutex), idle_(&idle), running_(&running) {}
    ~RunningGuard() {
        const std::scoped_lock lock(*mutex_);
        const auto it = std::find(running_->begin(), running_->end(), std::this_thread::get_id());
        if (it != running_->end()) {
            running_->erase(it);
        }
        idle_->notify_all();
    }
    RunningGuard(const RunningGuard &) = delete;
    RunningGuard &operator=(const RunningGuard &) = delete;
    RunningGuard(RunningGuard &&) = delete;
    RunningGuard &operator=(RunningGuard &&) = delete;

  private:
    std::mutex *mutex_;
    std::condition_variable *idle_;
    std::vector<std::thread::id> *running_;
};

} // namespace

InProcessEventBus::InProcessEventBus(ILogger &logger)
    : state_(std::make_shared<State>()), logger_(&logger) {}

Subscription InProcessEventBus::subscribe(std::string eventName, Handler handler) {
    if (eventName.empty() || !handler) {
        return Subscription{};
    }
    auto slot = std::make_shared<Slot>(std::move(eventName), std::move(handler));

    std::uint64_t id = 0;
    {
        const std::scoped_lock lock(state_->mutex);
        id = state_->nextId++;
        state_->slots.emplace(id, slot);
    }

    return Subscription([weakState = std::weak_ptr<State>(state_), id,
                         slot = std::move(slot)]() noexcept { cancelSlot(weakState, id, slot); });
}

void InProcessEventBus::cancelSlot(const std::weak_ptr<State> &weakState, std::uint64_t id,
                                   const std::shared_ptr<Slot> &slot) noexcept {
    try {
        if (const auto state = weakState.lock()) {
            const std::scoped_lock lock(state->mutex);
            state->slots.erase(id);
        }
        std::unique_lock lock(slot->mutex);
        slot->active = false;
        // Wait for handlers running on OTHER threads; a handler cancelling itself must not wait
        // for its own call to finish.
        const auto self = std::this_thread::get_id();
        slot->idle.wait(lock, [&] {
            return std::all_of(slot->running.begin(), slot->running.end(),
                               [&](const std::thread::id &t) { return t == self; });
        });
        if (slot->running.empty()) {
            slot->handler = nullptr; // release captured objects now
        }
    } catch (...) { // NOLINT(bugprone-empty-catch)
        // Only std::system_error from mutex/condition_variable on OS failure is possible here.
        // Cancelling must not throw (it runs in destructors); the slot stays inactive.
    }
}

void InProcessEventBus::invoke(Slot &slot, const DomainEvent &event) noexcept {
    {
        const std::scoped_lock lock(slot.mutex);
        if (!slot.active) {
            return;
        }
        slot.running.push_back(std::this_thread::get_id());
    }
    const RunningGuard guard(slot.mutex, slot.idle, slot.running);
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
    std::vector<std::shared_ptr<Slot>> matching;
    try {
        // Snapshot matching slots, then call handlers without holding the bus lock so handlers
        // may publish, subscribe or cancel.
        const std::scoped_lock lock(state_->mutex);
        for (const auto &[id, slot] : state_->slots) {
            if (slot->eventName == event.name()) {
                matching.push_back(slot);
            }
        }
    } catch (...) {
        Logger(*logger_, "core.event_bus").error("publish failed: out of memory or lock error");
        return;
    }
    for (const auto &slot : matching) {
        invoke(*slot, event);
    }
}

std::size_t InProcessEventBus::subscriberCount() const {
    const std::scoped_lock lock(state_->mutex);
    return state_->slots.size();
}

} // namespace fiaisdk::core
