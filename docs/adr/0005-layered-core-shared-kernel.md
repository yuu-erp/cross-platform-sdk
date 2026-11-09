# ADR-0005: `src/core` is layered (domain / application / infrastructure) and is the shared kernel

- Status: Accepted
- Date: 2026-11-09
- Amends: ADR-0001 (replaces the separate `src/kernel`)

## Context
`src/core` held only lifecycle classes, and shared building blocks lived in a separate
`src/kernel`. Modules need more common ground than `Result`/`Error`: identity and value types,
domain events, secret handling, a clock, logging and an event bus — each with a clear home so
modules do not reinvent them, and with the same layer rules modules follow.

## Options considered
1. Keep `src/kernel` (pure types) + flat `src/core` (lifecycle). Two places for shared code; no
   home for shared ports or their default adapters.
2. **Layer `src/core` like a module and fold `src/kernel` into `core/domain`.** One place for all
   shared code; the same mental model everywhere; the linker enforces the direction.

## Decision
Option 2.

| Layer / target | Holds | May use |
|---|---|---|
| `core/domain` → `fiaisdk_core_domain` | `Error`, `Result<T>`, `StrongType`, `DomainEvent`, `Timestamp`, `SecureBytes`, `constantTimeEquals` | std only |
| `core/application` → `fiaisdk_core_application` | ports `IClock`, `ILogger`, `IEventBus`; `Subscription`, `Logger`; `Module`, `ModuleContext`, `ModuleRegistry`, `SdkCore` | core/domain |
| `core/infrastructure` → `fiaisdk_core_infrastructure` | `SystemClock`, `NullLogger`, `InProcessEventBus` | core/application, OS, 3rd-party |
| `core/composition_root.cpp` (in `fiaisdk`) | wiring of adapters + modules | everything |

Module layers may use the core layer of the same or lower level: `<m>/domain` → `core/domain`;
`<m>/application` and `<m>/api` → `core/domain` + `core/application`; `<m>/infrastructure` →
all core layers. Header includes are enforced by `tools/check_layering.py` (CI); the CMake
target graph controls what is *linked* but cannot stop a header-only include, because every
internal target shares the `src/` include root.

What is deliberately **not** in core: generic `Entity`/`UseCase`/`Repository`/`ValueObject` base
classes (they add inheritance without behaviour; use cases are plain classes with `execute()`;
an entity is a class with a `StrongType` id), and anything only one module needs. Add a shared
type here only when a second module needs it.

## Consequences
+ One shared vocabulary; modules get clock/logger/event bus via `ModuleContext` (testable).
+ Shared fakes for core ports live in `tests/support/core/`.
- Changes in `core/domain` and `core/application` affect every module: review them as
  carefully as public API.
- `NullLogger` is the default sink until a logging ADR decides how logs reach the host app.
