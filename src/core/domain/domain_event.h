#pragma once

#include "core/domain/timestamp.h"

#include <string_view>

namespace fiaisdk::core {

/// Something that happened in the domain that other parts of the SDK may react to
/// ("wallet.created", "network.disconnected"). Published through the IEventBus port.
///
/// Concrete events: `static constexpr std::string_view kName = "<module>.<event>";`, return it
/// from name(), and carry only immutable data (no secrets, no references to mutable state).
class DomainEvent {
  public:
    virtual ~DomainEvent() = default;

    /// Stable, unique "<module>.<event>" identifier used for subscriptions.
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] Timestamp occurredAt() const noexcept {
        return occurredAt_;
    }

  protected:
    /// The time comes from the IClock port, never from the system clock directly.
    explicit DomainEvent(Timestamp occurredAt) noexcept : occurredAt_(occurredAt) {}

    DomainEvent(const DomainEvent &) = default;
    DomainEvent &operator=(const DomainEvent &) = default;
    DomainEvent(DomainEvent &&) = default;
    DomainEvent &operator=(DomainEvent &&) = default;

  private:
    Timestamp occurredAt_;
};

} // namespace fiaisdk::core
