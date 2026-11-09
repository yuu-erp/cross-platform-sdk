add_library(fiaisdk_core STATIC
    module_registry.cpp
    sdk_core.cpp
)
target_link_libraries(fiaisdk_core
    PUBLIC fiaisdk_kernel
    PRIVATE fiaisdk_compiler_options
)
