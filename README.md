# FiaiSDK

Modular, cross-platform (Windows / macOS / Linux) C++20 SDK for the Meta-Node ecosystem.
One shared library (`fiaisdk.dll`, `libfiaisdk.dylib`, `libfiaisdk.so`) exposing a **stable C ABI**
and a **header-only C++ wrapper** on top of it.

> Status: pre-1.0 scaffold. Modules `wallet`, `smc`, `network` are registered placeholders with
> no business features yet.

## Design in one paragraph

Modular monolith; each module follows Clean Architecture (`domain → application ← infrastructure`,
`api/` facade) — see [target architecture](docs/architecture/target-architecture.md) and
[ADRs](docs/adr/). Only `fiaisdk_*` C functions are exported (hourglass, ADR-0002); the C++ API in
`include/fiaisdk/cpp/` compiles inside your binary, so it is safe across compilers and runtimes.

## Requirements

- CMake ≥ 3.24, a C99 + C++20 compiler (GCC ≥ 11, Clang ≥ 14, MSVC 2022)
- Ninja (Linux/macOS presets); Visual Studio 2022 on Windows
- Optional: clang-format, clang-tidy, Python 3 (layering check)

## Build & test

```bash
# Linux / macOS
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan   # ASan + UBSan

# Windows (Developer PowerShell)
cmake --preset msvc && cmake --build --preset msvc-debug && ctest --preset msvc-debug
```

Checks used by CI and by AI agents:

```bash
python3 tools/check_layering.py
clang-format --dry-run --Werror $(git ls-files '*.h' '*.hpp' '*.c' '*.cpp')
clang-tidy -p build/debug $(git ls-files 'src/*.cpp')
```

## Use it

```cmake
find_package(FiaiSDK 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE FiaiSDK::fiaisdk)
```

C:

```c
#include <fiaisdk/fiaisdk.h>

fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
config.application_name = "my-app";
fiaisdk_sdk* sdk = NULL;
if (fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK) {
    /* ... */
    fiaisdk_sdk_destroy(sdk);
}
```

C++:

```cpp
#include <fiaisdk/cpp/sdk.hpp>

fiaisdk::Sdk sdk({"my-app", "1.0.0"});   // throws fiaisdk::Error on failure
bool hasWallet = sdk.hasModule("wallet");
```

Full reference: [docs/api/sdk.md](docs/api/sdk.md). Install: `cmake --install build/release --prefix <dir>`.

## Repository layout

```text
include/fiaisdk/        public C ABI (+ cpp/ header-only wrapper)
src/kernel/             Result/Error, pure shared code
src/core/               lifecycle, module registry, composition root
src/modules/<name>/     one folder per module (domain/ application/ infrastructure/ api/)
src/bindings/c_api/     extern "C" implementation
tests/                  abi/ (C99), cpp/ (wrapper), unit/, package/ (find_package consumer)
docs/                   architecture/, adr/, api/, development/
tools/                  check_layering.py
.claude/, AGENTS.md     AI agent rules, skills and reviewers (shared with the team)
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/development/adding-a-module.md](docs/development/adding-a-module.md).

## Still to decide (ADRs pending)

Test framework, dependency manager, async/threading model, logging, crypto / HTTP / JSON libraries.
