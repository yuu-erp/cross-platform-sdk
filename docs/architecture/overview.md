# Architecture overview (current state)

Target design and rules: [target-architecture.md](target-architecture.md). This file describes
what is implemented today.

```text
consumer (C / C++ / FFI)
        |  include/fiaisdk/*.h  (C ABI)      include/fiaisdk/cpp/*.hpp (header-only, on top of C ABI)
        v
src/bindings/c_api       sdk_c_api.cpp, status_mapping.cpp, handles.h  ┐
src/bootstrap/composition_root.cpp  (only place knowing concrete modules)   ┘ compiled into fiaisdk (SHARED)
        v
src/modules/crypto       hash/random/BLS/secp256k1 (reviewed libraries) -> fiaisdk_crypto_core/_infra (STATIC)
src/modules/network      demo vertical slice (all layers)       -> fiaisdk_network_core/_infra (STATIC)
src/modules/wallet       offline dual-key wallet (domain/application/api) -> fiaisdk_wallet (STATIC)
src/modules/smc/api      placeholder module -> fiaisdk_smc (STATIC)
src/core/infrastructure  SystemClock, NullLogger, InProcessEventBus -> fiaisdk_core_infrastructure (STATIC)
src/core/application     ports, Module, ModuleRegistry, SdkCore  -> fiaisdk_core_application    (STATIC)
src/core/domain          Result, Error, StrongType, DomainEvent, SecureBytes -> fiaisdk_core_domain (STATIC)
```

Lifecycle: `fiaisdk_sdk_create` → `composeSdk()` creates the core services (SystemClock,
NullLogger, InProcessEventBus) and registers modules (crypto, network, wallet, smc) →
`SdkCore::create` → `ModuleRegistry::initializeAll(ModuleContext)` in order; on failure,
already-initialised modules are shut down in reverse. `fiaisdk_sdk_destroy` → `~SdkCore` →
`shutdownAll()` in reverse order, then the services are destroyed (modules never outlive them).

Details of each core class: [src/core/README.md](../../src/core/README.md).

Module dependency graph: [module-graph.md](module-graph.md).
