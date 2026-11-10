# ADR-0008: SDK composition root lives in bootstrap

- Status: Accepted
- Date: 2026-11-11
- Amends: ADR-0005 (composition-root location)

## Context

Core is the shared kernel and must not depend on concrete modules. The composition root
assembles all modules and adapters; placing it under core required a special exception to
that boundary and obscured its SDK-wide role. The owner requested moving it to bootstrap.

## Decision

Move composition_root.h/cpp to src/bootstrap and use namespace fiaisdk::bootstrap for
composeSdk, ComposedSdk and ModuleApis. Continue compiling the implementation directly into
the fiaisdk shared library, alongside C bindings; no new library or generic bootstrap framework.

Only bindings enter bootstrap to create an SDK and obtain facade references. Core, modules
and platform must not include bootstrap. Only the composition root has permission to wire
all concrete adapters. The layering checker enforces this boundary and rejects flat core
source files, including the old composition-root location.

## Consequences

Core has no module-dependency exception. Composition root continues to configure services,
wire modules, register initialization order and return SdkCore with non-owning facade pointers.
Module lifecycle, ownership and behavior remain unchanged. Agent guidance and architecture
documentation use the new location.

## ABI / compatibility impact

None. Only internal paths, namespace and build wiring change. Public headers, exported C
functions, status values and struct layouts remain unchanged.
