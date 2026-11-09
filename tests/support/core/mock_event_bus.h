#pragma once

#include "core/application/ports/i_event_bus.h"

#include <gmock/gmock.h>

#include <string>

namespace fiaisdk::test {

/// gMock IEventBus for tests that must verify interactions ("publishes X once").
/// For tests that only need delivery, use the real core::InProcessEventBus instead.
class MockEventBus final : public core::IEventBus {
  public:
    MOCK_METHOD(core::Subscription, subscribe, (std::string eventName, Handler handler),
                (override));
    MOCK_METHOD(void, publish, (const core::DomainEvent &event), (noexcept, override));
};

} // namespace fiaisdk::test
