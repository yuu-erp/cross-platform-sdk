# Plan 0009: Typed API contract and review fixes

Executor: Codex (or any coding agent). Owner: project owner. Created: 2026-11-11.
Decision record: [ADR-0009](../adr/0009-public-api-data-contract.md).

## 0. Read first and ground rules

Read before editing: `AGENTS.md`, `docs/adr/0002-hourglass-c-abi.md`,
`docs/adr/0009-public-api-data-contract.md`, `.claude/rules/c-abi.md`,
`.claude/rules/platform.md`, `docs/development/workflow.md`.

Rules for this plan:
- Work in phase order. Each phase is one focused change and one suggested commit.
- **Do not commit or push unless the owner asks.** Suggested commit messages are given so the
  owner can commit phase by phase.
- Do not touch unrelated files. Do not reformat files you did not change.
- Do not claim a build or test passed unless you ran it. If a step cannot run (missing Qt,
  no Windows host), say so in the final report.
- The working tree already contains the owner's uncommitted network/TCP work. Keep it; only make
  the edits listed here.
- Items in section 11 need an owner decision. Do not implement them.

Before starting, the owner removes `.git/claude-review-wip.patch` (leftover from the review; it is
inside `.git`, so it is not tracked).

Baseline commands (run once at the start and record the results):
```bash
git status --short
cmake --preset debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build --preset debug
cmake --preset debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build/debug-clang \
  && cmake --build build/debug-clang && ctest --test-dir build/debug-clang --output-on-failure
```
Expected baseline: GCC fails in `library_crypto_provider.cpp`; clang builds and passes.

---

## Phase 1 — Fix the GCC build (CI linux-gcc / linux-asan / linux-tsan)

Problem: GCC 13 reports `-Werror=ignored-attributes` for
`std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_preallocated_destroy)>`
because the function carries a nonnull attribute.

File: `src/modules/crypto/infrastructure/library_crypto_provider.cpp`

Steps:
1. In the anonymous namespace add:
   ```cpp
   struct SecpContextDestroy {
       void operator()(secp256k1_context *context) const noexcept {
           secp256k1_context_preallocated_destroy(context);
       }
   };
   ```
2. Change the `secp` member of `Impl` to
   `std::unique_ptr<secp256k1_context, SecpContextDestroy> secp;`
   Keep it declared after `secpStorage` so it is destroyed first.
3. Leave the OpenSSL `unique_ptr`s unchanged (they compile on GCC).

Acceptance:
- `cmake --build` with GCC succeeds with `-Werror` on.
- Debug tests pass with GCC and clang.

Commit: `fix(crypto): use a functor deleter for the secp256k1 context`

---

## Phase 2 — Make the allocation fault-injection test ASan-clean

Problem: `tests/unit/core/infrastructure/event_bus_allocation_test.cpp` replaces
`operator new`/`delete` but not the `std::nothrow_t` forms. libstdc++ (`std::stable_sort` in
GoogleTest) allocates with the runtime's nothrow `new` and frees through the replaced `delete`
(`std::free`), so ASan reports alloc-dealloc-mismatch and three tests fail.

File: `tests/unit/core/infrastructure/event_bus_allocation_test.cpp`

Steps:
1. Add `#include <new>` if missing.
2. Next to the existing replacements, add:
   ```cpp
   void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
       try {
           return ::operator new(size);
       } catch (...) {
           return nullptr;
       }
   }
   void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept {
       return ::operator new(size, tag);
   }
   void operator delete(void *memory, const std::nothrow_t &) noexcept {
       std::free(memory);
   }
   void operator delete[](void *memory, const std::nothrow_t &) noexcept {
       std::free(memory);
   }
   ```
   The nothrow form shares the same allocation budget, which is the intended fault injection.
3. Do not add `-fsanitize` exceptions or `ASAN_OPTIONS=alloc_dealloc_mismatch=0`.

Acceptance:
- `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` passes with GCC,
  including all `EventBusAllocation.*` tests.

Commit: `test(core): replace nothrow allocation operators in the allocation test`

---

## Phase 3 — Remove the JSON network functions from the public ABI (ADR-0009)

These functions were never committed. Remove them completely.

Steps:
1. `include/fiaisdk/network.h`: delete the doc comment block starting
   "JSON variants of the Network API" and the four declarations
   `fiaisdk_network_connect_json`, `fiaisdk_network_disconnect_json`,
   `fiaisdk_network_get_status_json`, `fiaisdk_network_json_free`.
2. Delete the untracked file `src/bindings/c_api/network_json_c_api.cpp`.
3. `src/CMakeLists.txt`: remove `bindings/c_api/network_json_c_api.cpp` from the `fiaisdk`
   sources. While there, update the header comment of that file if it still describes the old
   target layout (it should list `fiaisdk_core_infrastructure`, `fiaisdk_crypto_infra`,
   `fiaisdk_network_infra`, `fiaisdk_wallet`, `fiaisdk_smc` and the bootstrap source).
4. `tests/abi/expected_exports.txt`: remove the four `*_json` lines.
5. `tests/abi/network_abi_test.c`: remove `test_json_responses` and its call in `main`.
   Then check that the typed tests cover the same behavior; add any missing typed checks:
   - `fiaisdk_network_get_status` reports `DISCONNECTED`, `connected_at_unix_ms == 0` and
     `latency_ms == 0` before connecting;
   - connecting to a URL without a port returns `FIAISDK_STATUS_INVALID_ARGUMENT`;
   - connecting to the loopback listener returns OK, then status is `CONNECTED`;
     `fiaisdk_network_get_endpoint` (two-call size protocol) returns the same URL;
   - `fiaisdk_network_disconnect` returns OK and status returns to `DISCONNECTED`.
6. `docs/api/network.md`: remove the JSON section (the "Qt client uses ..._json" paragraph, the
   JSON sample and the `fiaisdk_network_get_status_json` snippet). Replace it with a short
   typed example: create SDK → `fiaisdk_network_connect` → `fiaisdk_network_get_status` with
   `FIAISDK_NETWORK_STATUS_INIT` → `fiaisdk_network_get_endpoint` (two calls) → disconnect.
   State that TCP endpoints currently require a numeric IPv4/IPv6 host (no DNS resolution).
7. `grep -rn "_json" --exclude-dir=build --exclude-dir=.git .` must only return the Qt example
   (fixed in Phase 4) and nothing in `include/`, `src/`, `tests/` or `docs/`.

Acceptance:
- Build and all tests pass (GCC and clang, debug).
- `fiaisdk_exported_symbols` passes with the reduced list.

Commit: `refactor(abi): drop JSON network functions in favor of the typed API (ADR-0009)`

---

## Phase 4 — Port the Qt example to the typed API and add it to git

Files: `examples/qt/main.cpp`, `examples/qt/CMakeLists.txt`, `examples/qt/README.md`
(all currently untracked).

Steps:
1. In `struct Api`, replace the JSON function pointers with:
   `fiaisdk_network_connect`, `fiaisdk_network_disconnect`, `fiaisdk_network_get_status`,
   `fiaisdk_network_get_endpoint` (keep `version`, `status_message`, `sdk_create`,
   `sdk_destroy`). Update `resolve()` to bind exactly these symbols.
2. Remove `JsonReply`, `takeJson`, `readReply` and every `QJson*` include and use.
3. Connect (still on `QtConcurrent::run`, so the UI thread never blocks): return
   `{fiaisdk_status, elapsedMs}`. On the UI thread show `OK` or
   `fiaisdk_status_message(status)` with the numeric status.
4. Status refresh: fill `fiaisdk_network_status status = FIAISDK_NETWORK_STATUS_INIT;`, call
   `get_status`; when `CONNECTED`, read the endpoint with the two-call protocol
   (first call with `buffer = NULL` to get the size, then a `QByteArray` of that size) and
   display state, endpoint, connected-at (`QDateTime::fromMSecsSinceEpoch`) and latency.
5. Disconnect: call `fiaisdk_network_disconnect` and show the status.
6. Do not log or display anything beyond the endpoint the user typed (the endpoint may contain
   secrets per `network.h`; showing it in the user's own window is acceptable, writing it to a
   log file is not).
7. `examples/qt/README.md`: describe that the example is standalone (not built by the SDK's
   CMake or CI), requires Qt 6 Widgets/Concurrent, and how to configure it with
   `-DFIAISDK_LIBRARY=<path>`; mention it demonstrates the typed API only.
8. Keep `cxx_std_17` unless the code needs C++20.
9. `git add examples/qt` (staging is allowed; committing is the owner's call).

Acceptance:
- `grep -n "_json\|QJson" examples/qt/main.cpp` returns nothing.
- If Qt 6 is available: configure and build the example against `build/debug/lib`, run it, and
  connect to a local listener (`nc -l 127.0.0.1 4200`); otherwise report that the Qt build was not
  run.

Commit: `docs(examples): Qt network example uses the typed C API`

---

## Phase 5 — Platform must not depend on modules

Problem: `src/platform/tcp_socket.h` includes
`modules/network/application/ports/i_node_connector.h`, and the OS sockets derive from
`network::NodeConnection`. Platform is a lower layer; it must not know module types.
`tools/check_layering.py` does not catch this.

Target design:
```cpp
// src/platform/tcp_socket.h
#pragma once
#include "core/domain/result.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
namespace fiaisdk::platform {
/// A connected TCP socket. Destruction closes it. Implementations live in platform/<os>/.
class TcpSocket {
  public:
    virtual ~TcpSocket() = default;
    /// Known peer closure, without waiting or consuming data.
    [[nodiscard]] virtual bool peerClosed() const noexcept = 0;
};
struct TcpConnection {
    std::chrono::milliseconds latency{0};
    std::unique_ptr<TcpSocket> socket;
};
/// host is an unbracketed numeric IPv4/IPv6 address.
[[nodiscard]] core::Result<TcpConnection>
openTcpSocket(const std::string &host, std::uint16_t port, std::chrono::milliseconds timeout);
} // namespace fiaisdk::platform
```

Steps:
1. Rewrite `src/platform/tcp_socket.h` as above (adjust names only if they clash).
2. `src/platform/linux/tcp_socket.cpp` and `src/platform/windows/tcp_socket.cpp`: the local
   `Socket` class derives from `platform::TcpSocket`; `openTcpSocket` returns
   `TcpConnection{latency, std::move(socket)}`. No other behavior change.
3. `src/modules/network/infrastructure/tcp_node_connector.cpp`: add a file-local adapter
   ```cpp
   class TcpNodeConnection final : public NodeConnection {
     public:
       explicit TcpNodeConnection(std::unique_ptr<platform::TcpSocket> socket) noexcept
           : socket_(std::move(socket)) {}
       [[nodiscard]] bool peerClosed() const noexcept override { return socket_->peerClosed(); }
     private:
       std::unique_ptr<platform::TcpSocket> socket_;
   };
   ```
   and map `Result<TcpConnection>` to `Result<OpenedConnection>` (propagate the error unchanged;
   allocate the adapter inside the existing exception-safe path).
4. `tools/check_layering.py`, in `check_edge`, add before the final `return None`:
   ```python
   if src.kind in ("platform", "platform_os") and dst.kind in ("module", "bindings", "bootstrap"):
       return "platform must not depend on modules, bindings or bootstrap"
   ```
   (bootstrap is already rejected earlier; keep the message consistent.) Add cases for
   `platform -> modules` and `platform_os -> modules` to `tools/test_check_layering.py` and run
   `python3 tools/test_check_layering.py`.
5. Temporarily revert step 1 locally to confirm the checker now reports the old include, then
   restore it.
6. Update `.claude/rules/platform.md` with one line: platform code may depend on `core/domain`
   only, never on modules, bindings or bootstrap.

Acceptance:
- `python3 tools/check_layering.py` reports 0 violations.
- `grep -rn "modules/" src/platform` returns nothing.
- Network unit and ABI tests pass on GCC, clang, ASan and TSan.
- Windows code compiles in CI (cannot be verified locally on Linux; say so).

Commit: `refactor(platform): decouple TCP socket from network module types`

---

## Phase 6 — Write the data contract into rules and docs

Steps:
1. Add `docs/adr/0009-public-api-data-contract.md` (provided with this plan; already in place).
2. `.claude/rules/c-abi.md`, under "Headers", add a "Data conventions (ADR-0009)" list:
   binary data as `uint8_t*` + size (never hex/base64 text); 256-bit values as 32 big-endian
   bytes; instants `int64_t` Unix ms, durations `uint32_t` ms; booleans `uint8_t`; lists via
   two-call count protocol or an SDK-owned result handle; no JSON, no free-text error payloads;
   secrets only as byte buffers or opaque handles.
3. `docs/api/abi-guidelines.md`: add the same list in summary form with a link to ADR-0009.
4. `docs/architecture/target-architecture.md`: in the conventions section, link the ABI and
   Async bullets to ADR-0009; note that a message adapter, if ever built, lives outside the core
   library and calls only the public API.
5. `AGENTS.md`: in "Hard nos", add: "JSON, hex or other encoded text as the canonical form of
   structured or binary data in the public C ABI (ADR-0009)". Keep the file short.
6. `src/modules/network/README.md`: state numeric-host-only TCP and the typed status API.

Acceptance:
- Links resolve (`grep -rn "0009" docs AGENTS.md .claude/rules`).
- No code change in this phase.

Commit: `docs(abi): record the typed public API data contract (ADR-0009)`

---

## Phase 7 — Module lifecycle: crypto facade and wallet dependency

Problems:
- `CryptoModule` creates its facade in the constructor but destroys it in `shutdown()`
  (asymmetric with `WalletModule`, which creates in `initialize()`).
- `composition_root.cpp` dereferences `*crypto->facade()` before initialization and relies on
  registration order alone for wallet → crypto.

Steps:
1. `src/modules/crypto/api/crypto_module.cpp`: create `facade_` in `initialize()` (after the
   provider check), not in the constructor. Keep `shutdown()` resetting it.
2. `src/modules/wallet/api/wallet_module.h/.cpp`: take `const crypto::CryptoModule &` instead of
   `const crypto::CryptoFacade &`. In `initialize()`, read `crypto_->facade()`; if it is null
   return `core::Error::notInitialized("wallet requires an initialized crypto module")`
   (maps to `FIAISDK_STATUS_NOT_INITIALIZED`), otherwise create `WalletFacade`.
   This is allowed: wallet depends on `crypto/api` only.
3. `src/bootstrap/composition_root.cpp`: construct `WalletModule(*crypto)` and add a comment that
   crypto must be registered before wallet because the registry initializes in registration order
   and shuts down in reverse.
4. Tests (`tests/unit/wallet/...`): add a test that `WalletModule::initialize` fails with the
   not-initialized error when the crypto module has not been initialized, and succeeds after it
   has. Use the existing `unavailable_crypto_provider.h` or a real provider as appropriate.

Acceptance:
- All tests pass (GCC, clang, ASan, TSan).
- `check_layering.py` reports 0 violations.

Commit: `refactor(wallet): obtain the crypto facade during initialization`

---

## Phase 8 — Style and static analysis

Steps:
1. Run `clang-tidy -p build/debug-clang` on:
   `src/bindings/c_api/wallet_c_api.cpp`, `src/bindings/c_api/crypto_c_api.cpp`,
   `src/modules/crypto/application/*.h` (through a translation unit),
   `src/modules/crypto/infrastructure/library_crypto_provider.cpp`,
   `src/modules/wallet/application/*.h|*.cpp`,
   `src/modules/network/infrastructure/tcp_node_connector.cpp`,
   `src/platform/*/tcp_socket.cpp`.
   Fix `readability-braces-around-statements` (add braces to single-line `if`s) and any other
   finding that is not a documented, justified `NOLINT`. Do not change behavior.
2. Format only changed files with clang-format 18.1.8 (the CI version):
   ```bash
   python3 -m venv /tmp/cf && /tmp/cf/bin/pip install clang-format==18.1.8
   git diff --name-only HEAD -- '*.c' '*.h' '*.cpp' '*.hpp' | xargs /tmp/cf/bin/clang-format -i
   ```
   This includes `include/fiaisdk/network.h` (the `FIAISDK_NETWORK_STATUS_INIT` macro wraps
   differently between 18.1.3 and 18.1.8; follow 18.1.8).

Acceptance:
- `clang-format --dry-run --Werror` (18.1.8) is clean for all tracked C/C++ files.
- clang-tidy reports no new warnings on the listed files.

Commit: `style: braces and formatting in crypto, wallet and network bindings`

---

## Phase 9 — Full verification and report

Run and record each result:
```bash
python3 tools/check_layering.py
tools/check.sh full --fresh            # if it fits the local toolchain
cmake --preset debug -DCMAKE_C_COMPILER=gcc   -DCMAKE_CXX_COMPILER=g++     && cmake --build --preset debug && ctest --preset debug
cmake --preset debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build/debug-clang && cmake --build build/debug-clang && ctest --test-dir build/debug-clang
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan --repeat until-fail:3
git status --short
git diff --stat
```
Confirm `git status` shows no untracked file referenced by CMake
(`git ls-files --others --exclude-standard src include tests cmake` must be empty).

Final report (in Vietnamese for the owner): per phase — files changed, decisions, commands run,
results, anything not verified (Windows, macOS, Qt), and remaining work.

---

## 10. Out of scope

- Any message/JSON-RPC/protobuf adapter (needs its own ADR when a real host requires it).
- The asynchronous public API and executor ADR.
- DNS resolution for `tcp://hostname` endpoints.
- New public functions other than the test additions above.

## 11. Owner decisions (do not implement without approval)

1. Rename `src/platform/linux/` to `src/platform/posix/`, since macOS uses the same file.
   Touches `OS_DIRS` in `tools/check_layering.py`, network CMake, `AGENTS.md` rule 6 and
   `.claude/rules/platform.md`.
2. Whether `LibraryCryptoProvider` initialization failure should keep failing the whole
   `fiaisdk_sdk_create` (current behavior) or only disable crypto/wallet. If it stays, document it
   in ADR-0006.

---

## Phase 10 — Review follow-ups (added 2026-11-11 after review of phases 1–9)

Review result: approve after these three fixes. GCC 13.3 x64 debug/asan/tsan and clang 18 were
verified by the reviewer (123/123). Do not change behavior.

### 10.1 Restore `#pragma once` and silence the new clang-tidy check
Cause: newer clang-tidy (LLVM 20+) reports `portability-avoid-pragma-once` because `.clang-tidy`
enables `portability-*`. The project rule (`.claude/rules/cpp-style.md`) is `#pragma once`.
Steps:
1. `.clang-tidy`: add `-portability-avoid-pragma-once,` on the line after `portability-*,`.
2. Convert these 12 headers back to `#pragma once` (remove the `#ifndef/#define/#endif` guard and
   its trailing comment; keep everything else byte-identical):
   - `src/modules/crypto/application/{derive_public_key,generate_private_key,generate_random_bytes,hash_keccak256,hash_sha256,key_validation,sign_data,verify_signature}.h`
   - `src/modules/crypto/application/ports/i_crypto_provider.h`
   - `src/modules/wallet/application/{create_wallet,import_wallet,sign_with_wallet}.h`
3. Acceptance: `grep -rL "pragma once" src --include=*.h` prints nothing; clang-tidy on the
   crypto/wallet translation units reports no `portability-avoid-pragma-once`.

### 10.2 Make `ICryptoProvider` non-copyable
File: `src/modules/crypto/application/ports/i_crypto_provider.h`. A polymorphic interface must
not be copyable (C++ Core Guidelines C.67); match `INodeConnector`:
```cpp
ICryptoProvider() = default;
ICryptoProvider(const ICryptoProvider &) = delete;
ICryptoProvider &operator=(const ICryptoProvider &) = delete;
ICryptoProvider(ICryptoProvider &&) = delete;
ICryptoProvider &operator=(ICryptoProvider &&) = delete;
virtual ~ICryptoProvider() = default;
```
If a test fake fails to compile because it was copied, construct it in place instead
(`std::make_unique`/reference) — do not re-enable copying.

### 10.3 Fix the pre-existing format diff that keeps CI lint red
Run clang-format **18.1.8** (same venv as Phase 8) with `-i` on `include/fiaisdk/types.h`
only. Then `clang-format --dry-run --Werror` (18.1.8) over all tracked C/C++ files must be clean,
and `tools/check.sh full --fresh` must get past the format step.

### 10.4 Verify and report
Run: GCC and clang debug, asan, tsan (`--repeat until-fail:3`), `python3 tools/check_layering.py`,
`python3 tools/test_check_layering.py`, `tools/check.sh full --fresh`. Report in Vietnamese:
files changed, commands, results, anything not verified. Do not commit or push.

Suggested commit: `style: restore #pragma once, non-copyable crypto port, format types.h`
