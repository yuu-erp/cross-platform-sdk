---
name: write-adr
description: Write an Architecture Decision Record in docs/adr for a technical choice (dependency, threading model, error model, ABI policy, build tooling). Use when the user is choosing between options or a rule requires "decide in an ADR".
---
# Write ADR

1. `ls docs/adr` → next number `NNNN` (4 digits). File `docs/adr/NNNN-kebab-title.md` from
   `docs/adr/0000-template.md`.
2. Context: facts and constraints of THIS SDK (3 OSes, C ABI, single binary, security).
3. At least 2 real options with honest pros/cons; include license, maintenance, binary size,
   cross-platform support for dependencies. Verify facts (versions, licenses) — do not guess.
4. Decision + scope. Consequences incl. follow-up tasks. ABI impact.
5. Status `Proposed` unless the user explicitly accepts. Link it from related docs/rules.
6. Summarize options in Vietnamese for the owner and ask for the decision.

Pending ADRs to expect: test framework, dependency manager (vcpkg vs FetchContent),
error type (`Result<T>` vs C++23 `std::expected`), async/threading model, logging, crypto library,
HTTP/WebSocket client, JSON library.
