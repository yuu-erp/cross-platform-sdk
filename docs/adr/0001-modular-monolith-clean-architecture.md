# ADR-0001: Modular monolith with per-module Clean Architecture

- Status: Accepted
- Date: 2026-11-09

## Context
The SDK will host many feature areas (wallet, smc, network, auth, crypto, socket, storage,
notification, platform). They must be testable without network/OS, swappable at the
infrastructure edge, and shipped as one binary to keep distribution simple.

## Options considered
1. Flat modules (current scaffold) — simple, but business logic and I/O mix quickly.
2. Global horizontal layers (`src/domain`, `src/application`, ...) — module boundaries blur,
   every feature touches every folder.
3. **Vertical modules, each with domain/application/infrastructure/api** — boundaries clear,
   Clean Architecture applied where it matters, still one library.

## Decision
Option 3. Shared code lives in `src/kernel` (pure) and `src/platform` (OS ports). Layering is
enforced by CMake targets and `tools/check_layering.py`.

Clean Architecture is applied **proportionally**: a module with no real business rules (e.g. thin
network transport) may have only `application/` + `infrastructure/` + `api/`. Do not create empty
layers to look uniform.

## Consequences
+ Use cases unit-testable with fakes. + Adapters swappable (HTTP lib, key store).
- More files and interfaces; reviewers must reject speculative interfaces.
