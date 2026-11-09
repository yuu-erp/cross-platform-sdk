# fiaisdk_network_core : domain + application + api (no I/O, no third-party)
# fiaisdk_network_infra: infrastructure adapters (here: the demo SimulatedNodeProbe)
add_library(fiaisdk_network_core STATIC
    domain/endpoint.cpp
    application/connect_to_node.cpp
    application/disconnect_from_node.cpp
    api/network_facade.cpp
    api/network_module.cpp
)
target_link_libraries(fiaisdk_network_core
    PUBLIC fiaisdk_core_application
    PRIVATE fiaisdk_compiler_options
)

add_library(fiaisdk_network_infra STATIC
    infrastructure/simulated_node_probe.cpp
)
target_link_libraries(fiaisdk_network_infra
    PUBLIC fiaisdk_network_core
    PRIVATE fiaisdk_compiler_options
)
