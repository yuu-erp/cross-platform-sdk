add_library(fiaisdk_wallet STATIC
    api/wallet_module.cpp
)
target_link_libraries(fiaisdk_wallet
    PUBLIC fiaisdk_core
    PRIVATE fiaisdk_compiler_options
)
