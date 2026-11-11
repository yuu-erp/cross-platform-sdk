# fiaisdk_network_core : domain + application + api (no I/O, no third-party)
# fiaisdk_network_infra: concrete network adapters
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
    infrastructure/tcp_node_connector.cpp
)
target_link_libraries(fiaisdk_network_infra
    PUBLIC fiaisdk_network_core
    PRIVATE fiaisdk_compiler_options
)

if(WIN32)
    target_sources(fiaisdk_network_infra PRIVATE ${PROJECT_SOURCE_DIR}/src/platform/windows/tcp_socket.cpp)
    target_link_libraries(fiaisdk_network_infra PRIVATE ws2_32)
else()
    target_sources(fiaisdk_network_infra PRIVATE ${PROJECT_SOURCE_DIR}/src/platform/linux/tcp_socket.cpp)
endif()
