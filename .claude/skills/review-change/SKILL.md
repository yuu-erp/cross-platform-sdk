---
name: review-change
description: Review a diff/PR in this SDK against architecture, ABI, security, threading and test rules. Use when asked to review code, a branch, a PR, or "check this before merge".
---
# Review change

1. Get the diff (`git diff <base>...HEAD` or the PR). Group by area: abi / core / module / cmake / tests.
2. Checklist — cite file:line for each finding:
   - Layering: domain/application including infra/OS/3rd-party? cross-module internals? (run `tools/check_layering.py`)
   - ABI: exported C++/STL? enum in signature? struct layout change? status values changed? missing try/catch? ownership documented?
   - Errors: ignored `Result`? exception crossing boundary? `noexcept` that can throw?
   - Security: secrets in logs/strings, custom crypto, input not validated, TLS off.
   - Concurrency: callbacks under lock, lifetime after destroy, unowned threads.
   - Tests: behavior covered? regression test for fix? deterministic?
   - Speculative abstractions, dead code, unrelated reformatting.
3. Rank: **Blocker** / **Should fix** / **Nit**. No praise padding.
4. For changes in `include/fiaisdk/` or `src/bindings/` also delegate to `abi-reviewer`.
