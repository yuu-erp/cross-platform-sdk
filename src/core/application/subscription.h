#pragma once

#include <functional>
#include <utility>

namespace fiaisdk::core {

/// RAII handle for an event subscription: unsubscribes on destruction or cancel().
/// Move-only. Keep it as a member of the object whose `this` the handler captures: cancel()
/// blocks until a handler running on another thread has finished (IEventBus contract), so the
/// object can be destroyed right after.
class [[nodiscard]] Subscription {
  public:
    Subscription() = default;
    /// `cancel` must not throw; it is called at most once.
    explicit Subscription(std::function<void()> cancel) noexcept : cancel_(std::move(cancel)) {}

    ~Subscription() {
        cancel();
    }

    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;

    Subscription(Subscription &&other) noexcept : cancel_(std::exchange(other.cancel_, nullptr)) {}
    Subscription &operator=(Subscription &&other) noexcept {
        if (this != &other) {
            cancel();
            cancel_ = std::exchange(other.cancel_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] bool active() const noexcept {
        return static_cast<bool>(cancel_);
    }

    void cancel() noexcept {
        if (cancel_) {
            const auto cancelOnce = std::exchange(cancel_, nullptr);
            cancelOnce();
        }
    }

  private:
    std::function<void()> cancel_;
};

} // namespace fiaisdk::core
