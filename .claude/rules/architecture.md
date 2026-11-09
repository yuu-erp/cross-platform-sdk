---
paths:
  - "src/**"
---
# Architecture rules (src/)

Reference: `docs/architecture/target-architecture.md` §3 dependency table; shared building blocks
in `src/core/README.md` (use them, do not re-implement clock/logger/event bus/strong ids/secrets).

- Put code in the layer that owns the concept:
  - business rule / invariant / value object → `modules/<m>/domain/`
  - orchestration of one user intent → `modules/<m>/application/<verb>_<noun>.{h,cpp}` (use case)
  - "I need X from outside" → abstract port in `modules/<m>/application/ports/`
  - HTTP, JSON, disk, 3rd-party lib, OS call → `modules/<m>/infrastructure/` implementing a port
  - what bindings call → `modules/<m>/api/<m>_facade.{h,cpp}`
- Domain and application: no `<windows.h>`, `<unistd.h>`, sockets, file I/O, threads, clocks,
  random, logging backends, JSON libs. Inject them through ports (`IClock`, `IRandom`, `ILogger`).
- A port is defined by the consumer (application), named for the need (`IKeyStore`), not the tech.
- One use case = one class with one public `execute(...)` returning `Result<T>`; inputs are
  plain structs; no shared mutable state between use cases.
- Core (`src/core/`) never contains module business logic; add to core only what ≥ 2 modules need.
  Only `composition_root.cpp` includes concrete infrastructure classes and modules.
- Modules get shared services from `core::ModuleContext` in `initialize()`, never from globals.
- Cross-module reactions go through `core::IEventBus` (`subscribeTo<Event>`), not direct calls into
  another module's internals.
- Cross-module: include `modules/<other>/api/*.h` only. Never `domain/`, `application/`,
  `infrastructure/` of another module. No cycles between modules (document the graph in
  `docs/architecture/module-graph.md` when adding an edge).
- Do not create an interface with a single implementation unless it is a port at a layer
  boundary (those exist for testability). Do not create empty layers.
- After moving/adding includes run `python3 tools/check_layering.py`.
