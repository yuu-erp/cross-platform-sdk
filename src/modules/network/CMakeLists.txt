add_library(fiaisdk_network STATIC
    api/network_module.cpp
)
target_link_libraries(fiaisdk_network
    PUBLIC fiaisdk_core
    PRIVATE fiaisdk_compiler_options
)
