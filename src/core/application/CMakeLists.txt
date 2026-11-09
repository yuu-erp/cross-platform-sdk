# Core application: shared ports (IClock, ILogger, IEventBus), module contract and lifecycle.
# Depends on core domain only. Concrete adapters live in core/infrastructure.
add_library(fiaisdk_core_application STATIC
    module_registry.cpp
    sdk_core.cpp
)
target_link_libraries(fiaisdk_core_application
    PUBLIC fiaisdk_core_domain
    PRIVATE fiaisdk_compiler_options
)
