# Core infrastructure: default adapters for the core ports (clock, logger, event bus).
# The only core layer allowed to use OS / third-party APIs.
find_package(Threads REQUIRED)

add_library(fiaisdk_core_infrastructure STATIC
    in_process_event_bus.cpp
    system_clock.cpp
)
target_link_libraries(fiaisdk_core_infrastructure
    PUBLIC fiaisdk_core_application Threads::Threads
    PRIVATE fiaisdk_compiler_options
)
