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
