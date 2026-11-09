---
name: new-module
description: Scaffold a new SDK module (e.g. auth, storage, socket) with domain/application/infrastructure/api layers, CMake targets, registration, public C header and tests. Use when the user asks to add or create a module.
---
# New module

Input: module name `<m>` (snake_case) and its responsibility in one sentence. If the
responsibility or its dependencies on other modules are unclear, ask before creating files.

## Steps
1. Read `docs/architecture/target-architecture.md` and one existing module for conventions.
2. Decide which layers are really needed (ADR-0001: proportional). Write the decision in the
   module README.
3. Create:
   ```text
   src/modules/<m>/
     README.md                 responsibility, public use cases, dependencies, thread-safety
     CMakeLists.txt            fiaisdk_<m>_core + fiaisdk_<m>_infra
     domain/                   (only if real business rules)
     application/ports/
     infrastructure/
     api/<m>_facade.{h,cpp}    + <m>_module.{h,cpp} implementing core Module (init/shutdown)
   include/fiaisdk/<m>.h       only if consumers need it now
   src/bindings/c_api/<m>_c_api.cpp
   tests/unit/<m>/  tests/abi/<m>_abi_test.c
   ```
4. Register in `src/core/composition_root.cpp` (only place that knows concrete adapters).
5. `add_subdirectory(src/modules/<m>)` and link into `fiaisdk`.
6. Add the dependency edge(s) to `docs/architecture/module-graph.md`; ensure no cycle.
7. Run the `verify` skill.
8. Run the `architecture-reviewer` subagent; fix findings.

Do not add placeholder use cases or empty interfaces "for later".
