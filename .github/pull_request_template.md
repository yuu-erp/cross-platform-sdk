## Summary
<!-- What and why. Link issue/ADR. -->

## Type / scope
<!-- feat|fix|refactor|test|docs|build|ci(scope) -->

## Checklist
- [ ] `tools/check.sh full` passed locally (paste the summary below)
- [ ] Builds on: [ ] Linux [ ] macOS [ ] Windows (state which were actually run)
- [ ] `ctest` passes; new behavior has tests (regression test for fixes)
- [ ] `python3 tools/check_layering.py` clean
- [ ] clang-format / clang-tidy clean on changed files
- [ ] Public C ABI changed? [ ] No [ ] Additive [ ] Breaking (explain) — ABI test + `docs/api` updated
- [ ] Security-sensitive (wallet/auth/crypto/storage)? impact described below
- [ ] New dependency? ADR linked

## Testing performed
<!-- Commands + results -->

## Security / ABI notes
