# FiaiSDK

Modular, cross-platform (Windows / macOS / Linux) C++20 SDK for the Meta-Node ecosystem.
One shared library (`fiaisdk.dll`, `libfiaisdk.dylib`, `libfiaisdk.so`) exposing a **stable C ABI**
and a **header-only C++ wrapper** on top of it.

> Status: pre-1.0. `network` is a complete **demo** vertical slice (connect / disconnect / status,
> simulated transport). `crypto` provides hash/random/BLS/secp256k1 operations;
> `wallet` provides an in-memory offline dual-key wallet; `smc` remains a placeholder.

## Design in one paragraph

Modular monolith; each module follows Clean Architecture (`domain → application ← infrastructure`,
`api/` facade) — see [target architecture](docs/architecture/target-architecture.md) and
[ADRs](docs/adr/). Only `fiaisdk_*` C functions are exported (hourglass, ADR-0002); the C++ API in
`include/fiaisdk/cpp/` compiles inside your binary, so it is safe across compilers and runtimes.

## Requirements

- CMake ≥ 3.25, a C99 + C++20 compiler (GCC ≥ 11, Clang ≥ 14, MSVC 2022 or 2026)
- Ninja (Linux/macOS presets); Visual Studio on Windows (the `msvc` preset picks the newest installed)
- Optional: clang-format **18.1.8** (`pipx install clang-format==18.1.8`), clang-tidy, Python 3

## Build & test

```bash
# Linux / macOS
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan   # ASan + UBSan
cmake --preset tsan  && cmake --build --preset tsan  && ctest --preset tsan   # ThreadSanitizer

# Windows (Developer PowerShell)
cmake --preset msvc && cmake --build --preset msvc-debug && ctest --preset msvc-debug
```

Same checks as CI in one command (see [docs/development/workflow.md](docs/development/workflow.md)):

```bash
tools/check.sh          # before every commit
tools/check.sh full     # before push: + ASan, TSan, clang-tidy, install/find_package
```

Individual checks used by CI and by AI agents:

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

Full reference: [docs/api/sdk.md](docs/api/sdk.md), [docs/api/network.md](docs/api/network.md).
Testing: [docs/development/testing.md](docs/development/testing.md). Install: `cmake --install build/release --prefix <dir>`.

## Repository layout

```text
include/fiaisdk/        public C ABI (+ cpp/ header-only wrapper)
src/core/               shared kernel: domain/ application/ infrastructure/ + composition root
src/modules/<name>/     one folder per module (domain/ application/ infrastructure/ api/)
src/bindings/c_api/     extern "C" implementation
tests/                  abi/ (C99), cpp/ (wrapper), unit/ (GoogleTest), support/, package/
docs/                   architecture/, adr/, api/, development/
tools/                  check_layering.py
.claude/, AGENTS.md     AI agent rules, skills and reviewers (shared with the team)
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/development/adding-a-module.md](docs/development/adding-a-module.md).

## Testing

GoogleTest + GoogleMock v1.17.0 (ADR-0004). CMake uses an installed GTest ≥ 1.17 if found,
otherwise downloads the pinned commit; offline: `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<checkout>`.
`ctest --preset debug -R SdkCore` runs one suite.

## Still to decide (ADRs pending)

Runtime crypto providers: OpenSSL >=3.2, blst and libsecp256k1 (statically linked), see
[crypto API and build requirements](docs/api/crypto.md) and [ADR-0006](docs/adr/0006-minimal-crypto-provider.md).
Wallet key/address formats are specified in ADR-0007. Persistence/mnemonics, other runtime libraries, async/threading and logging remain open.

The offline wallet supports independent BLS/secp256k1 keys, two addresses, import, signing and
verification without private-key export or persistence. See [wallet API](docs/api/wallet.md).
Build hosts need OpenSSL >=3.2 static libcrypto and initial network access for pinned signing
libraries; see [crypto build requirements](docs/api/crypto.md).
