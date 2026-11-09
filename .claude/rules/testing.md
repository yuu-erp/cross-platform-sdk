---
paths:
  - "tests/**"
  - "**/*_test.cpp"
---
# Testing rules

- Framework: GoogleTest + GoogleMock (ADR-0004). Register with `fiaisdk_add_gtest()` in
  `tests/CMakeLists.txt`. Exception: `tests/abi/*.c` stay plain C99 (`test_check.h`).
- Layout mirrors src: `tests/unit/<core|module>/<layer>/<file>_test.cpp`.
- Reuse `tests/support/core/` doubles (`FakeClock`, `RecordingLogger`, `MockEventBus`); prefer
  fakes, use gMock only to verify interactions.
- `tests/unit/<module>/`: domain + use cases with hand-written fakes for ports. No network,
  disk, sleep, real clock, or randomness without a seed. Must run < 1s per file.
- `tests/integration/`: adapters, composition root, lifecycle (create → use → destroy, repeated).
- `tests/abi/`: compiled as **C**; covers NULL args, buffer-too-small, struct_size older/newer,
  destroy(NULL), status message for every code.
- `TEST(<Subject>, <ConditionExpectation>)` in CamelCase, no underscores (GoogleTest FAQ).
  One behavior per test.
- Bug fix → regression test that fails before the fix.
- Never paste real keys/tokens; use test vectors.
- Report only results you actually ran.
