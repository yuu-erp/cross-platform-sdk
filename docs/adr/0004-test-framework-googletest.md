# ADR-0004: Test framework — GoogleTest + GoogleMock

- Status: Accepted
- Date: 2026-11-09

## Context
Clean Architecture (ADR-0001) means most logic is tested through **ports replaced by fakes or
mocks**. The SDK builds with MSVC, Apple Clang, GCC and Clang, runs tests via CTest, and has a
C ABI that must also be tested from plain C. Until now tests used a hand-written `CHECK` macro.

## Options considered
1. **GoogleTest + GoogleMock** — de-facto standard for C++ projects; mocking of port interfaces is
   built in (gMock); first-class CMake integration (`gtest_discover_tests`, imported
   `GTest::` targets); works on all our compilers; BSD-3-Clause. Cons: heavier to build than
   header-only frameworks; v1.17 requires C++17 (we use C++20, so fine).
2. **Catch2 v3** — expressive `SECTION`/BDD style, nice output, BSL-1.0. Cons: no mocking library,
   so a second dependency (e.g. trompeloeil) would be needed for ports.
3. **doctest** — header-only and very fast to compile. Cons: no mocking either; smaller ecosystem.

## Decision
- GoogleTest + GoogleMock **v1.17.0**, pinned to commit `52eb8108c5bdec04579160ae17225d66034bd723`.
- Obtained by `cmake/GoogleTest.cmake`: `find_package(GTest 1.17)` first (system/vcpkg/Homebrew),
  otherwise `FetchContent` of the pinned commit. Test-only dependency: never linked into
  `fiaisdk`, never installed.
- Tests are registered with `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)` (no test binary
  runs at build time — important for Windows DLL lookup and cross builds).
- **Exception:** `tests/abi/*.c` stay plain C99 with `tests/support/test_check.h`, because they
  exist to prove the public headers and ABI work from C.
- Fakes for core ports live in `tests/support/core/` and are reused by module tests; use gMock
  (`MOCK_METHOD`) only when a test must verify interactions, prefer hand-written fakes otherwise.

## Consequences
+ One framework for unit/integration tests; mocks for ports without extra libraries.
+ Per-test CTest entries → `ctest -R <Suite>` and parallel `ctest -j`.
- First configure without a system GTest downloads googletest (needs network to github.com).
- The general dependency-manager decision (vcpkg vs FetchContent for runtime libraries) is still
  open; this ADR only covers the test framework.

## ABI / compatibility impact
None (test-only).
