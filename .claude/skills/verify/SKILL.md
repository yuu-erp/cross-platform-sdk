---
name: verify
description: Build, test, format-check, lint and layering-check the SDK and report real results. Use before declaring any code change done, or when the user asks to check/verify/build/run tests.
---
# Verify

Run what is available; for each step report the exact command and outcome. If a tool is missing,
say so — never report a step as passed that was not run.

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
