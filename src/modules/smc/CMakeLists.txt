add_library(fiaisdk_smc STATIC
    api/smc_module.cpp
)
target_link_libraries(fiaisdk_smc
    PUBLIC fiaisdk_core
    PRIVATE fiaisdk_compiler_options
)
