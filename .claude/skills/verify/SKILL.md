---
name: verify
description: Build, test, format-check, lint and layering-check the SDK and report real results. Use before declaring any code change done, or when the user asks to check/verify/build/run tests.
---
# Verify

Run what is available; for each step report the exact command and outcome.
Full guide for humans: `docs/development/testing.md`. If a tool is missing,
say so — never report a step as passed that was not run.

Preferred: one command that mirrors CI and stops at the first failure:

```bash
tools/check.sh          # layering + clang-format + debug build (-Werror) + all tests
tools/check.sh full     # + ASan/UBSan + TSan + clang-tidy + install/find_package
```

Use `full` for changes to the C ABI, bindings, threading code or CMake (see the table in
`docs/development/workflow.md`). Manual steps, if the script cannot run:

```bash
cmake --preset debug
cmake --build --preset debug 2>&1 | tail -50
ctest --preset debug
python3 tools/check_layering.py
git diff --name-only -- '*.h' '*.hpp' '*.cpp' '*.c' | xargs -r clang-format --dry-run --Werror
git diff --name-only -- '*.cpp' | xargs -r clang-tidy -p build/debug --quiet
```

Also check:
- New warnings introduced? (treat as failures)
- Public header changed? → ABI test exists + `docs/api` updated + abi-reviewer run.
- Only one OS verified? → state that the other OSes are unverified.

Report format (Vietnamese to owner):
| Bước | Lệnh | Kết quả |
