# AGENTS.md — FiaiSDK

Canonical instructions for AI coding agents (Claude Code, Cursor, Copilot, Codex...) and a
quick reference for humans. Keep it short; details live in `docs/` and `.claude/rules/`.

## What this is
Modular, cross-platform (Windows/macOS/Linux) **C++20 SDK** for the Meta-Node ecosystem,
shipped as **one shared library** exposing a **stable C ABI** plus a header-only C++ wrapper.
Status: pre-1.0, being rebuilt. Wallet supports offline signing, network supports TCP transport, and
SMC is a placeholder. Read the code before assuming a feature exists.

## Read before non-trivial work
1. `docs/architecture/target-architecture.md` — layout, layer rules, gap list; `src/core/README.md` — shared building blocks
2. `docs/adr/` — decisions (Clean Architecture per module, hourglass ABI, naming)
3. The module you touch + its tests

## Architecture in 6 rules
1. `include/fiaisdk/*.h` = public C ABI (only exported symbols). `include/fiaisdk/cpp/` = header-only wrapper over it.
2. Each module: `domain/ → application/ (use cases + ports) ← infrastructure/ (adapters)`, `api/` facade.
   `src/core` is the shared kernel, layered the same way — read `src/core/README.md` before adding shared code.
3. Dependencies point inward. Domain/application never include infrastructure, OS headers or 3rd-party I/O libs.
4. Modules talk only via another module's `api/` facade or the core EventBus — never its internals.
5. Only `src/bootstrap/composition_root.cpp` wires concrete adapters.
6. Platform-specific code only in `src/platform/{windows,macos,linux}/` behind a port.

## Commands
```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug   # Linux/macOS
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan    # ASan+UBSan
cmake --preset tsan  && cmake --build --preset tsan  && ctest --preset tsan    # ThreadSanitizer
cmake --preset msvc  && cmake --build --preset msvc-debug && ctest --preset msvc-debug  # Windows
tools/check.sh [full] [--fresh]            # everything CI checks, stops at first failure
python3 tools/check_layering.py            # dependency rules
# what to re-run after which change: docs/development/workflow.md; tests: docs/development/testing.md
clang-format --dry-run --Werror <files>    # formatting
clang-tidy -p build/debug <files>          # static analysis
```

## Conventions
- Names: see ADR-0003 (`fiaisdk_` C prefix, `FIAISDK_` macros, `fiaisdk::` namespace, `snake_case` files).
- Errors: `fiaisdk::core::Result<T>` internally; status codes at the C ABI; no exception crosses a module or ABI boundary.
- Commits: Conventional Commits `type(scope): description`; scopes = module names, `core`, `abi`, `cmake`, `ci`, `docs`.
- Every behavior change ships with tests; public API change also updates `docs/api/` and an example.

## Hard nos
- Exporting C++ classes/STL types; changing layout of an existing public struct; reordering status codes.
- JSON, hex or other encoded text as the canonical form of structured or binary data in the public C ABI (ADR-0009).
- Custom crypto primitives; logging keys/seeds/tokens; disabling TLS/signature verification.
- New third-party dependency without an ADR. New layer/interface without a second real use.
- Claiming build/test success without running it. Commits/pushes unless asked.
