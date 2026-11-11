# Target architecture (v1)

Status: **Accepted**. The 2026-11-09 refactor implements items 1–5, 7–11 of the gap list (§5);
module layers (item 6) are added per module with its first use case.
Decisions behind it: [ADR-0001](../adr/0001-modular-monolith-clean-architecture.md),
[ADR-0002](../adr/0002-hourglass-c-abi.md), [ADR-0003](../adr/0003-naming-and-prefixes.md),
[ADR-0004](../adr/0004-test-framework-googletest.md), [ADR-0005](../adr/0005-layered-core-shared-kernel.md).

## 1. Shape

One shared library (`fiaisdk.dll` / `libfiaisdk.dylib` / `libfiaisdk.so`) built from
internal static/object targets. Modules are boundaries inside the library, not binaries.

```text
 Consumers: C, C#, Swift, Kotlin/JNI, Python, Qt app
        |                     |
        |          C++ header-only wrapper  (include/fiaisdk/cpp/*.hpp)
        |                     |
        +-------- STABLE C ABI (include/fiaisdk/*.h) --------+   <- only exported symbols
                              |
                 src/bindings/c_api   (handles, error mapping, catch-all)
                              |
                 src/bootstrap/composition_root.cpp   (wires adapters + modules)
                              |
     +------------------------+------------------------+
  modules/wallet          modules/smc            modules/network ...
   api/ -> application/ -> domain/   <- infrastructure/ (implements ports)
                              |
                 src/core     shared kernel, layered the same way (ADR-0005):
                   domain/         Result, Error, StrongType, DomainEvent, SecureBytes
                   application/    ports IClock/ILogger/IEventBus, Module, lifecycle, SdkCore
                   infrastructure/ SystemClock, NullLogger, InProcessEventBus
                 src/platform (OS abstractions: windows/ macos/ linux/)
```

## 2. Directory layout

```text
include/fiaisdk/              PUBLIC, stable C ABI. C99-compatible headers only.
  fiaisdk.h                   umbrella header
  export.h                    FIAISDK_API / FIAISDK_CALL / FIAISDK_DEPRECATED
  types.h                     fiaisdk_status + codes, handles, config structs
  version.h                   generated from cmake/version.h.in
  sdk.h                       create/destroy/version/has_module
  <module>.h                  one header per public module, added with its first public function
  cpp/                        header-only C++17 wrapper over the C ABI (no exported symbols)
src/
  core/                       shared kernel, see src/core/README.md
    domain/ application/ infrastructure/
  bootstrap/                  SDK assembly: composition_root.{h,cpp} (not part of core targets)
  platform/                   ports for OS services + windows/ macos/ linux/ implementations
  modules/<name>/
    domain/                   entities, value objects, rules, events. Uses core/domain only.
    application/              use cases + ports it needs. Uses core/domain + core/application.
      ports/                  e.g. IRpcClient, IKeyStore
    infrastructure/           adapters implementing ports (HTTP, storage, crypto libs).
    api/                      module facade used by bindings + Module impl (init/shutdown)
    CMakeLists.txt            fiaisdk_<name> (later split per layer)
  bindings/c_api/             extern "C" implementation, one .cpp per public header
tests/
  abi/                        C99 tests of the C ABI + exported-symbol check
  cpp/                        GoogleTest tests of the C++ wrapper (C++17)
  unit/<area>/<layer>/        GoogleTest unit tests mirroring src/ (e.g. unit/core/domain)
  support/core/               shared test doubles: FakeClock, RecordingLogger, MockEventBus
  package/                    find_package(FiaiSDK) consumer
examples/  c/  cpp/
docs/  architecture/  adr/  api/  development/
tools/  check_layering.py  (CI + AI verification of dependency rules)
```

## 3. Dependency rules (enforced)

| Layer | May include / link | Must NOT include |
|---|---|---|
| `core/domain` | std only | any other `src/` code, I/O, OS, 3rd-party |
| `core/application` | `core/domain` | `core/infrastructure`, modules, platform, bindings |
| `core/infrastructure` | `core/application`, `core/domain`, `platform`, 3rd-party | modules, bindings |
| `bootstrap/composition_root.{h,cpp}` | core services, module APIs/adapters, platform implementations | — |
| `<m>/domain` | own `domain`, `core/domain` | own application/infrastructure/api, `core/application`, platform, other modules |
| `<m>/application` | own `domain`, `core/domain`, `core/application`, other modules' `api/` | infrastructure, `core/infrastructure`, platform, bindings |
| `<m>/infrastructure` | own `domain`/`application`, all `core/` layers, `platform`, 3rd-party | own `api`, bindings, other modules' internals |
| `<m>/api` | own `domain`/`application`, `core/domain`, `core/application` | infrastructure (wired by composition root), bindings |
| `bindings/c_api` | `include/fiaisdk`, `bootstrap`, `core`, module `api/` | module internals |

Enforcement: (1) `tools/check_layering.py` checks every `#include` (CI + AI verify step);
(2) the CMake target graph (`fiaisdk_core_domain` ← `_application` ← `_infrastructure`) controls
linking. All internal targets share the `src/` include root, so (1) is what catches header-only
violations.

Core/modules/platform must not depend on `bootstrap`; only the C bindings enter the assembly
layer to create the SDK and obtain facade references. Its namespace is `fiaisdk::bootstrap`.

## 4. Key policies

- **ABI** ([ADR-0009](../adr/0009-public-api-data-contract.md)): only typed C functions are exported.
  C++ API is header-only inline on top of the C ABI. A future message adapter lives outside the
  core library and calls only the public API.
- **Errors**: internal `fiaisdk::core::Result<T>` (until C++23 `std::expected` is allowed). No exceptions
  across module boundaries; every C entry point has `try { } catch (...)` mapping to a status code.
- **Status codes**: `typedef int32_t fiaisdk_status;` + `#define`/enum constants — never pass a C
  `enum` type by value across ABI (size is implementation-defined).
- **Extensible structs**: first field `uint32_t struct_size`; accept any size `>=` the v1 size;
  never reorder or remove fields; append only.
- **Memory**: caller-allocated buffers with `size_t* inout_len` pattern, or SDK-owned objects freed
  by a matching `fiaisdk_<x>_free`. Never `free()` across the boundary.
- **Async** ([ADR-0009](../adr/0009-public-api-data-contract.md)): C ABI uses `callback + void* user_data + request id`; callbacks run on an SDK worker
  thread (documented per function); cancellation via `fiaisdk_cancel(request_id)`. Decide in an ADR
  before the first async API.
- **Composition root** ([ADR-0008](../adr/0008-bootstrap-composition-root.md)): only `src/bootstrap/composition_root.cpp` knows concrete adapters.

## 5. Gap list vs original scaffold (status after 2026-11-09 refactor)

1. ✅ Build broken: headers in `include/fiaisdk/` but code includes `<mysdk/...>`.
2. ✅ Naming mixed → all `fiaisdk` (ADR-0003).
3. ✅ C++ class export removed; `include/fiaisdk/cpp/sdk.hpp` is header-only; link-time export list.
4. ✅ C API → `bootstrap::composeSdk` → `SdkCore` (no public C++ in between).
5. ✅ `Module::initialize/shutdown`, ordered init with rollback; only `composition_root.cpp` knows modules.
6. ⏳ Per-module layers now exist for network, crypto and wallet; smc remains a lifecycle placeholder.
7. ✅ `fiaisdk_compiler_options` linked by every target; `-Werror` in debug/asan/msvc presets.
8. ✅ Registry lookup compares `std::string_view`, no allocation.
9. ✅ `FIAISDK_SDK_CONFIG_V1_SIZE` + normalisation; tested with a larger future struct.
10. ✅ `version.h` generated from `project(VERSION)`; `fiaisdk_version_number()`.
11. ✅ ABI (C99), wrapper, unit, export-symbol and `find_package` tests; GoogleTest (ADR-0004).
