---
paths:
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cpp"
---
# C++ style

- C++20; follow `.clang-format` (LLVM base: 4 spaces, 100 cols, `T &x` pointer style, short functions
  expanded) and `.clang-tidy`. Format only files you change.
- Naming (ADR-0003): files `snake_case`, types `PascalCase`, functions `camelCase`,
  members `name_`, constants `kName`, macros `FIAISDK_NAME`.
- `#pragma once`. Include order: own header, project, third-party, std (clang-format sorts).
- Ownership: values > `std::unique_ptr` > `std::shared_ptr` (justify). No owning raw pointers, no `new`/`delete`.
- Errors: `fiaisdk::kernel::Result<T>` (`src/kernel/result.h`); exceptions only for programming errors inside a layer and never
  across modules/ABI. `noexcept` only when nothing inside can throw (no `std::string` allocation).
- `[[nodiscard]]` on functions returning `Result`, status or handles. `const` by default.
- Strong types for ids/amounts/addresses (`WalletId`, `Amount`) — not raw `std::string`/`uint64_t`.
- Money/token amounts: integers in smallest unit (or a vetted big-int); never floating point.
- No global mutable state, no singletons; pass dependencies via constructor.
- Avoid macros except export/platform detection. No `using namespace` in headers.
