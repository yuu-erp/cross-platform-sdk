# ADR-0003: Naming and prefixes

- Status: Accepted (2026-11-09, product name confirmed: fiaisdk)
- Date: 2026-11-09

## Decision
| Thing | Name |
|---|---|
| CMake project / package | `FiaiSDK` (`find_package(FiaiSDK)`) |
| Library target / alias | `fiaisdk` / `FiaiSDK::fiaisdk` |
| Public include dir | `include/fiaisdk/` → `#include <fiaisdk/sdk.h>` |
| C functions | `fiaisdk_<module>_<verb>[_<noun>]` e.g. `fiaisdk_wallet_get_balance` |
| C types | `fiaisdk_<name>` (no `_t` suffix, reserved by POSIX), handles opaque |
| Macros / constants | `FIAISDK_` (`FIAISDK_API`, `FIAISDK_STATUS_OK`) |
| CMake options | `FIAISDK_BUILD_TESTS`, `FIAISDK_BUILD_EXAMPLES`, ... |
| C++ namespace | public wrapper `fiaisdk::` (header-only); internal `fiaisdk::kernel`, `fiaisdk::core`, `fiaisdk::bindings`, modules `fiaisdk::<module>` |
| Files | `snake_case.{h,hpp,cpp}`; classes `PascalCase`; functions/methods `camelCase`; members `name_` |
| Shared library file | `fiaisdk.dll` / `libfiaisdk.dylib` / `libfiaisdk.so.0.<minor>` (pre-1.0 SONAME) |

All `mysdk`/`MYSDK_`/`CrossPlatformSDK` names were replaced in the 2026-11-09 refactor.
