# Testing guide

How to build and run every test locally, what each suite covers, and how to add tests.
CI (`.github/workflows/ci.yml`) runs the same commands on Linux (GCC, Clang, ASan, TSan),
macOS and Windows (MSVC).

## 1. Prerequisites

| | macOS | Linux (Ubuntu/Debian) | Windows |
|---|---|---|---|
| CMake ≥ 3.25 | `brew install cmake` | `sudo apt install cmake` (or pip/Kitware repo) | Visual Studio installer / cmake.org |
| Generator | `brew install ninja` | `sudo apt install ninja-build` | Visual Studio 2022 or 2026 (C++ workload) |
| Compiler | Xcode command line tools | `gcc g++` or `clang` | MSVC (from Visual Studio) |
| clang-format 18.1.8 | `pipx install clang-format==18.1.8` | same | same |
| Python 3 | preinstalled | preinstalled | python.org |

GoogleTest is downloaded automatically on the first configure (pinned v1.17.0, ADR-0004).
Offline: clone `https://github.com/google/googletest` at tag `v1.17.0` and pass
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest`.

## 2. Run everything

```bash
# macOS / Linux
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Windows (Developer PowerShell or plain PowerShell)
cmake --preset msvc
cmake --build --preset msvc-debug
ctest --preset msvc-debug
```

Expected: `100% tests passed`. Build output lives in `build/<preset>/`.

## 3. What the suites cover

| CTest name(s) | Source | Language | Covers |
|---|---|---|---|
| `fiaisdk_abi_test` | `tests/abi/sdk_abi_test.c` | C99 | SDK create/destroy, status codes, config struct versions |
| `fiaisdk_network_abi_test` | `tests/abi/network_abi_test.c` | C99 | network C API: NULL args, struct layout, size protocol, lifecycle |
| `fiaisdk_exported_symbols` | `tests/abi/check_exports.cmake` | CMake | the DLL/.so/.dylib exports exactly `tests/abi/expected_exports.txt` |
| `SdkWrapper.*`, `NetworkWrapper.*` | `tests/cpp/*` | C++17 | header-only C++ wrapper as a consumer uses it, thread safety |
| `Result.*`, `StrongType.*`, `SecureBytes.*`, ... | `tests/unit/core/**` | C++20 | core domain / application / infrastructure |
| `Endpoint.*`, `ConnectToNodeTest.*`, ... | `tests/unit/network/**` | C++20 | network module, layer by layer |
| `fiaisdk_example_*` | `examples/` | C, C++ | examples run (and fail on an unreachable node) |

Each GoogleTest case is its own CTest entry (`gtest_discover_tests`), so filters work per case.

## 4. Run a subset

```bash
ctest --preset debug -R Endpoint                  # all Endpoint.* cases
ctest --preset debug -R "ConnectToNode|Network"   # regex
ctest --preset debug -R network_abi --output-on-failure
ctest --preset debug -j 8                         # parallel
ctest --preset debug --repeat until-fail:50 -R EventBus   # hunt flaky tests

# Or call a GoogleTest binary directly (more detail, gtest flags):
./build/debug/bin/fiaisdk_network_tests --gtest_filter='ConnectToNodeTest.*'
./build/debug/bin/fiaisdk_network_tests --gtest_list_tests
```

On Windows the binaries are in `build\msvc\bin\Debug\`.

## 5. Sanitizers (Linux / macOS)

```bash
cmake --preset asan && cmake --build --preset asan && ctest --preset asan   # memory + UB
cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan   # data races
```

A sanitizer finding makes the test fail and prints a stack trace.

## 6. Try the network demo by hand

```bash
./build/debug/bin/fiaisdk_example_c                                   # default endpoint
./build/debug/bin/fiaisdk_example_c "wss://NODE.example.com:8546/ws"  # normalised URL
./build/debug/bin/fiaisdk_example_c https://down.invalid              # unreachable → exit 1
./build/debug/bin/fiaisdk_example_cpp http://node.example.com         # TLS policy → exit 1
```

The transport is simulated: `*.invalid` hosts are unreachable, others answer in 5–49 ms.

## 7. Writing tests

**Where:** mirror `src/`: `tests/unit/<core|module>/<layer>/<file>_test.cpp`, C ABI tests in
`tests/abi/<module>_abi_test.c`, wrapper tests in `tests/cpp/`. Register new files in
`tests/CMakeLists.txt` (`fiaisdk_add_gtest(...)` or the C pattern next to `fiaisdk_abi_test`).

**Naming:** `TEST(<Subject>, <ConditionExpectation>)` in CamelCase, one behaviour per test.

**Test doubles** (in `tests/support/`, include as `"support/..."`):

| Double | For |
|---|---|
| `test::FakeClock` | deterministic time: `advance(5s)`, `setNow(t)` |
| `test::RecordingLogger` | assert on log lines (and that secrets are never logged) |
| `test::MockEventBus` (gMock) | verify a module subscribes/publishes |
| real `core::InProcessEventBus` | capture published events with `subscribeTo<Event>` |
| `test::FakeNodeProbe` | script probe results: `willSucceed(42ms)`, `willFail(Error::unavailable(...))` |

**Use-case test pattern** (no network, no sleeps, runs in microseconds):

```cpp
class ConnectToNodeTest : public ::testing::Test {
  protected:
    test::FakeClock clock_;
    test::RecordingLogger logger_;
    core::InProcessEventBus bus_{logger_};
    test::FakeNodeProbe probe_;
    NetworkSession session_;
    ConnectToNode connect_{probe_, clock_, bus_, session_};
};

TEST_F(ConnectToNodeTest, UnreachableNodeReturnsUnavailableAndStaysDisconnected) {
    probe_.willFail(core::Error::unavailable("down"));
    const auto result = connect_.execute({"https://node.example.com", 1s});
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, core::ErrorCode::Unavailable);
    EXPECT_FALSE(session_.current());
}
```

**Rules:** deterministic (no real clock, network, disk, unseeded randomness); a bug fix comes
with a test that fails before the fix; a new public C function comes with a C ABI test and a
line in `tests/abi/expected_exports.txt`.

## 8. Before pushing (same checks as CI)

```bash
ctest --preset debug
python3 tools/check_layering.py
git ls-files '*.h' '*.hpp' '*.c' '*.cpp' | xargs clang-format --dry-run --Werror
clang-tidy -p build/debug $(git ls-files 'src/*.cpp')     # optional locally, recommended
```

## 9. Troubleshooting

| Symptom | Fix |
|---|---|
| `CMAKE_MAKE_PROGRAM-NOTFOUND` / "Ninja not found" | install Ninja, then `rm -rf build/<preset>` and configure again |
| `Could not create named generator Visual Studio 17 2022` | old preset cache: delete `build/msvc` |
| first configure hangs / fails downloading googletest | no access to github.com: use `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` (section 1) |
| CI `lint` fails but local check passes | local clang-format is not 18.1.8: `clang-format --version` |
| Windows: test exits with 0xc0000135 | DLL not found: run from `build\msvc\bin\Debug` (presets already put exe and DLL together) |
