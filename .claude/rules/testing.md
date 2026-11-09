---
paths:
  - "tests/**"
  - "**/*_test.cpp"
---
# Testing rules

- Framework: the one chosen in ADR (default proposal: GoogleTest via FetchContent/vcpkg).
- `tests/unit/<module>/`: domain + use cases with hand-written fakes for ports. No network,
  disk, sleep, real clock, or randomness without a seed. Must run < 1s per file.
- `tests/integration/`: adapters, composition root, lifecycle (create → use → destroy, repeated).
- `tests/abi/`: compiled as **C**; covers NULL args, buffer-too-small, struct_size older/newer,
  destroy(NULL), status message for every code.
- Name tests `Subject_Condition_Expectation`. One behavior per test.
- Bug fix → regression test that fails before the fix.
- Never paste real keys/tokens; use test vectors.
- Report only results you actually ran.
