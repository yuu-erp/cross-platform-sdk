# Target architecture (v1)

Status: **Accepted**. The 2026-11-09 refactor implements items 1–5, 7–11 of the gap list (§5);
module layers (item 6) are added per module with its first use case.
Decisions behind it: [ADR-0001](../adr/0001-modular-monolith-clean-architecture.md),
[ADR-0002](../adr/0002-hourglass-c-abi.md), [ADR-0003](../adr/0003-naming-and-prefixes.md).

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
                 src/core     (lifecycle, config, registry, composition root, event bus)
                              |
     +------------------------+------------------------+
  modules/wallet          modules/smc            modules/network ...
   api/ -> application/ -> domain/   <- infrastructure/ (implements ports)
                              |
                 src/kernel   (Result/Error, strong types, no I/O)
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
  cpp/                        header-only C++20 wrapper over the C ABI (no exported symbols)
    sdk.hpp  wallet.hpp ...
src/
  kernel/                     shared kernel: Result<T>, Error, ids, byte buffers. No I/O, no OS.
  platform/                   ports for OS services + windows/ macos/ linux/ implementations
  core/                       Sdk lifecycle, ModuleRegistry, Config, EventBus, composition_root.cpp
  modules/<name>/
    domain/                   entities, value objects, domain rules/errors. Depends on kernel only.
    application/              use cases + ports (abstract interfaces it needs). Depends on domain.
      ports/                  e.g. IRpcClient, IKeyStore, IClock
    infrastructure/           adapters that implement ports (HTTP, storage, 3rd-party crypto libs)
    api/                      module facade used by bindings + Module impl (init/shutdown)
    CMakeLists.txt            defines fiaisdk_<name>_core (domain+application) and _infra
  bindings/c_api/             extern "C" implementation, one .cpp per public header
tests/
  unit/<module>/              domain + application tests with fake ports (no network, no disk)
  integration/                adapters, cross-module flows, core lifecycle
  abi/                        C-only tests compiled as C; struct layout/size checks; exported-symbol check
examples/  c/  cpp/
docs/  architecture/  adr/  api/  development/
tools/  check_layering.py  (CI + AI verification of dependency rules)
```

## 3. Dependency rules (enforced)

| Layer | May include / link | Must NOT include |
|---|---|---|
| `kernel` | std only | anything in `src/` |
| `<m>/domain` | `kernel` | application, infrastructure, api, core, platform, other modules, 3rd-party I/O libs |
| `<m>/application` | `<m>/domain`, `kernel` | infrastructure, api, bindings, concrete libs |
| `<m>/infrastructure` | `<m>/application` (ports), `<m>/domain`, `kernel`, `platform`, 3rd-party libs | `<m>/api`, bindings, other modules' internals |
| `<m>/api` | `<m>/application`, `<m>/domain`, `core/module.h` | infrastructure (wired by composition root), bindings |
| `core` | `kernel`, `platform` ports, module `api/` only in `composition_root.cpp` | module domain/application/infrastructure |
| `bindings/c_api` | `include/fiaisdk`, `core`, module `api/` | module domain internals directly |
| other module | `<other>/api` public facade or core EventBus | `<other>/domain|application|infrastructure` |

Enforcement: (1) CMake — `_core` targets do not link 3rd-party I/O libs, so infra headers do not
resolve; (2) `tools/check_layering.py` in CI and in the AI verify step.

## 4. Key policies

- **ABI**: only C functions are exported. C++ API is header-only inline on top of C ABI (hourglass).
- **Errors**: internal `fiaisdk::Result<T>` (until C++23 `std::expected` is allowed). No exceptions
  across module boundaries; every C entry point has `try { } catch (...)` mapping to a status code.
- **Status codes**: `typedef int32_t fiaisdk_status;` + `#define`/enum constants — never pass a C
  `enum` type by value across ABI (size is implementation-defined).
- **Extensible structs**: first field `uint32_t struct_size`; accept any size `>=` the v1 size;
  never reorder or remove fields; append only.
- **Memory**: caller-allocated buffers with `size_t* inout_len` pattern, or SDK-owned objects freed
  by a matching `fiaisdk_<x>_free`. Never `free()` across the boundary.
- **Async**: C ABI uses `callback + void* user_data + request id`; callbacks run on an SDK worker
  thread (documented per function); cancellation via `fiaisdk_cancel(request_id)`. Decide in an ADR
  before the first async API.
- **Composition root**: only `src/core/composition_root.cpp` knows concrete adapters.

## 5. Gap list vs original scaffold (status after 2026-11-09 refactor)

1. ✅ Build broken: headers in `include/fiaisdk/` but code includes `<mysdk/...>`.
2. ✅ Naming mixed → all `fiaisdk` (ADR-0003).
3. ✅ C++ class export removed; `include/fiaisdk/cpp/sdk.hpp` is header-only; link-time export list.
4. ✅ C API → `core::composeSdk` → `SdkCore` (no public C++ in between).
5. ✅ `Module::initialize/shutdown`, ordered init with rollback; only `composition_root.cpp` knows modules.
6. ⏳ No domain/application/infrastructure layers yet (no use cases yet — by design).
7. ✅ `fiaisdk_compiler_options` linked by every target; `-Werror` in debug/asan/msvc presets.
8. ✅ Registry lookup compares `std::string_view`, no allocation.
9. ✅ `FIAISDK_SDK_CONFIG_V1_SIZE` + normalisation; tested with a larger future struct.
10. ✅ `version.h` generated from `project(VERSION)`; `fiaisdk_version_number()`.
11. ✅/⏳ ABI (C99), wrapper, unit, export-symbol and `find_package` tests added; framework ADR pending.
