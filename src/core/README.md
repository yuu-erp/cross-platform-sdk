# src/core — shared kernel of the SDK

Layered exactly like a module (ADR-0005). Code here is used by **every** module, so add to it
only what at least two modules need, and review changes like public API.

```text
core/
├── domain/            pure types, std only                      -> fiaisdk_core_domain
│   ├── error.h            Error, ErrorCode (+ factories)
│   ├── result.h           Result<T> / Result<void>  (no exceptions between layers/modules)
│   ├── strong_type.h      StrongType<T, Tag>  (WalletId, Address… cannot be mixed up)
│   ├── domain_event.h     DomainEvent  (name() "<module>.<event>", occurredAt())
│   ├── timestamp.h        Timestamp (wall clock), MonotonicTime
│   └── secure_bytes.h     SecureBytes, secureZero, constantTimeEquals  (secrets)
├── application/       ports + lifecycle, depends on domain      -> fiaisdk_core_application
│   ├── ports/
│   │   ├── i_clock.h      IClock      — current time (fake it in tests)
│   │   ├── i_logger.h     ILogger, LogLevel, LogRecord
│   │   └── i_event_bus.h  IEventBus + subscribeTo<Event>()
│   ├── subscription.h     Subscription  (RAII unsubscribe)
│   ├── logger.h           Logger  (category front-end over ILogger)
│   ├── module.h           Module  (name / initialize(ModuleContext) / shutdown)
│   ├── module_context.h   ModuleContext  (clock, logger, eventBus given to modules)
│   ├── module_registry.*  ordered init, rollback on failure, reverse shutdown
│   └── sdk_core.*         SdkCore  (config + CoreServices + modules of one instance)
├── infrastructure/    default adapters, may use OS/3rd-party    -> fiaisdk_core_infrastructure
│   ├── system_clock.*     SystemClock : IClock
│   ├── null_logger.h      NullLogger : ILogger  (default until the logging ADR)
│   └── in_process_event_bus.*  InProcessEventBus : IEventBus  (sync, thread-safe)
└── composition_root.* the only file that knows concrete adapters and modules
```

## Which core layer may I use?

| From | core/domain | core/application | core/infrastructure |
|---|---|---|---|
| `core/domain`, `<module>/domain` | ✅ | ❌ | ❌ |
| `core/application`, `<module>/application`, `<module>/api` | ✅ | ✅ | ❌ |
| `core/infrastructure`, `<module>/infrastructure` | ✅ | ✅ | ✅ |
| `core/composition_root.cpp` | ✅ | ✅ | ✅ (+ modules) |

Checked by `python3 tools/check_layering.py` (every `#include`, in CI). The CMake target graph
only controls linking: all internal targets share the `src/` include root.

## How a module uses core

```cpp
// modules/wallet/domain/wallet_id.h
struct WalletIdTag;
using WalletId = core::StrongType<std::string, WalletIdTag>;

// modules/wallet/domain/wallet_created.h
class WalletCreated final : public core::DomainEvent {
  public:
    static constexpr std::string_view kName = "wallet.created";
    WalletCreated(core::Timestamp at, WalletId id) : DomainEvent(at), id_(std::move(id)) {}
    std::string_view name() const noexcept override { return kName; }
    ...
};

// modules/wallet/api/wallet_module.cpp
core::Result<void> WalletModule::initialize(const core::ModuleContext& context) {
    context_ = context;               // copy it: ModuleContext is a small copyable handle
    clock_ = &context.clock();
    onConnected_ = core::subscribeTo<network::Connected>(context.eventBus(), [this](auto& e) {...});
    return {};
}
```

Tests use the shared doubles in `tests/support/core/`: `FakeClock`, `RecordingLogger`,
`MockEventBus` (gMock), or the real `InProcessEventBus`.

## Reusing the event bus

Use the `IEventBus` supplied by `ModuleContext`; do not create a bus per module. Define an
immutable event derived from `DomainEvent`, with a unique `kName` (`"<module>.<event>"`) and a
timestamp supplied by `IClock`. Use `subscribeTo<Event>()` for typed delivery, and store the
returned `Subscription` in the receiving module. Publish events after releasing module locks:
handlers run synchronously and may call back into the publisher.

```cpp
// During initialize(context):
onConnected_ = core::subscribeTo<network::NetworkConnected>(
    context.eventBus(), [this](const network::NetworkConnected& event) {
        // Copy the data needed by this module; do not retain a reference to event.
    });

// At the start of shutdown(), before destroying state captured by the handler:
onConnected_.cancel();
```

Cancel explicitly before destroying callback-visible state. A subscription member's destructor
alone is insufficient if other members have already been destroyed. Different publishing
threads may invoke the same handler concurrently: synchronize its mutable state. Do not cancel
mutually dependent running handlers from each other (see `IEventBus`'s deadlock restriction).
Destroy the bus/SDK only after all publishing/subscribing threads have stopped.

`InProcessEventBus` indexes subscriptions by event name and shares immutable per-topic snapshots.
For `T` topics and `K` matching subscriptions, publish costs `O(log T + K)` instead of scanning
all subscriptions; it performs no bus-owned heap allocation on normal delivery. Handler code
and exception logging can still allocate. Subscribe/cancel copy a topic's snapshot (`O(K)`),
so this implementation favours frequent publishing and relatively stable subscriptions. An
in-flight publish keeps its old snapshot alive; a cancelled handler is skipped even if present
in that snapshot. New subscribers participate in subsequent (including nested) publishes.

Subscribe is transactional: allocation failure exposes no handler. Cancellation deactivates
first, waits for other threads' calls, and destroys captures outside bus locks. Self-cancellation
releases captures after the last invocation returns. If snapshot pruning runs out of memory,
inactive slot metadata may remain until a later topic update; callbacks stay disabled and their
captures are still released. Delivery is synchronous and best effort, without persistence,
retries, a worker pool, or global ordering between publishing threads.

Allocation-failure tests run in `fiaisdk_event_bus_allocation_tests`, a separate executable so
its replacement allocator does not affect other test suites.
