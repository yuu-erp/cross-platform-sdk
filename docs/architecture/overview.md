# Architecture overview (current state)

Target design and rules: [target-architecture.md](target-architecture.md). This file describes
what is implemented today.

```text
consumer (C / C++ / FFI)
        |  include/fiaisdk/*.h  (C ABI)      include/fiaisdk/cpp/*.hpp (header-only, on top of C ABI)
        v
src/bindings/c_api       sdk_c_api.cpp, status_mapping.cpp, handles.h  ┐
src/core/composition_root.cpp  (only place knowing concrete modules)   ┘ compiled into fiaisdk (SHARED)
        v
src/core                 SdkCore, ModuleRegistry, Module         -> fiaisdk_core   (STATIC)
src/modules/{network,wallet,smc}/api   placeholder modules       -> fiaisdk_<name> (STATIC)
src/kernel               Result<T>, Error                        -> fiaisdk_kernel (INTERFACE)
```

Lifecycle: `fiaisdk_sdk_create` → `composeSdk()` registers modules (network, wallet, smc) →
`ModuleRegistry::initializeAll()` in order; on failure, already-initialised modules are shut down in
reverse. `fiaisdk_sdk_destroy` → `~SdkCore` → `shutdownAll()` in reverse order.

Module dependency graph: [module-graph.md](module-graph.md).
