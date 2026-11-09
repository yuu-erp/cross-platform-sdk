#pragma once

#include "modules/network/domain/connection_info.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace fiaisdk::network {

/// Current connection of one SDK instance. Thread-safe.
///
/// Two locks: the operation lock serialises connect/disconnect (which may block on I/O), while
/// current() stays cheap and never waits for a probe in progress.
///
/// Every state change gets the next sequence number. Events are published after the locks are
/// released, so subscribers on different threads may observe them out of order; the sequence
/// lets them discard stale events (or they simply re-read the status).
class NetworkSession {
  public:
    /// RAII ownership of the operation gate (see lockOperations). Move-only.
    class [[nodiscard]] OperationLock {
      public:
        OperationLock() = default;
        explicit OperationLock(NetworkSession &session) noexcept : session_(&session) {}
        ~OperationLock() {
            release();
        }
        OperationLock(const OperationLock &) = delete;
        OperationLock &operator=(const OperationLock &) = delete;
        OperationLock(OperationLock &&other) noexcept
            : session_(std::exchange(other.session_, nullptr)) {}
        OperationLock &operator=(OperationLock &&other) noexcept {
            if (this != &other) {
                release();
                session_ = std::exchange(other.session_, nullptr);
            }
            return *this;
        }

        [[nodiscard]] bool ownsLock() const noexcept {
            return session_ != nullptr;
        }

      private:
        void release() noexcept {
            if (session_ != nullptr) {
                session_->releaseGate();
                session_ = nullptr;
            }
        }
        NetworkSession *session_ = nullptr;
    };

    [[nodiscard]] std::optional<ConnectionInfo> current() const {
        const std::scoped_lock lock(stateMutex_);
        return current_;
    }

    /// Stores `info`; returns the sequence number of this change.
    std::uint64_t
    set(ConnectionInfo info) noexcept(std::is_nothrow_move_assignable_v<ConnectionInfo>) {
        const std::scoped_lock lock(stateMutex_);
        current_ = std::move(info);
        return ++sequence_;
    }

    struct Replaced {
        std::uint64_t disconnectSequence;
        std::uint64_t connectSequence;
    };

    /// Atomically replaces the current connection with `info` (readers never see a gap).
    /// Consumes two sequence numbers: one for the implied disconnect, one for the connect.
    Replaced
    replace(ConnectionInfo info) noexcept(std::is_nothrow_move_assignable_v<ConnectionInfo>) {
        const std::scoped_lock lock(stateMutex_);
        current_ = std::move(info);
        const std::uint64_t disconnect = ++sequence_;
        return {disconnect, ++sequence_};
    }

    /// Clears the connection; returns the sequence number of this change.
    std::uint64_t clear() noexcept {
        const std::scoped_lock lock(stateMutex_);
        current_.reset();
        return ++sequence_;
    }

    /// Serialise connect/disconnect. Waits at most `timeout`; on timeout the returned lock does
    /// not own the gate (ownsLock() == false).
    [[nodiscard]] OperationLock lockOperations(std::chrono::milliseconds timeout) {
        std::unique_lock lock(gateMutex_);
        if (!gateFree_.wait_for(lock, timeout, [this] { return !busy_; })) {
            return OperationLock{};
        }
        busy_ = true;
        return OperationLock(*this);
    }

    /// Serialise connect/disconnect, waiting as long as needed.
    [[nodiscard]] OperationLock lockOperations() {
        std::unique_lock lock(gateMutex_);
        gateFree_.wait(lock, [this] { return !busy_; });
        busy_ = true;
        return OperationLock(*this);
    }

  private:
    void releaseGate() noexcept {
        {
            const std::scoped_lock lock(gateMutex_);
            busy_ = false;
        }
        gateFree_.notify_one();
    }

    // Operation gate: a mutex + condition variable instead of std::timed_mutex, whose timed
    // locking is invisible to some sanitizers and differs between standard libraries.
    std::mutex gateMutex_;
    std::condition_variable gateFree_;
    bool busy_ = false;

    mutable std::mutex stateMutex_;
    std::optional<ConnectionInfo> current_;
    std::uint64_t sequence_ = 0;
};

} // namespace fiaisdk::network
