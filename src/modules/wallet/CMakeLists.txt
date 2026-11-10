add_library(fiaisdk_wallet STATIC
    api/wallet_module.cpp
    application/import_wallet.cpp)
target_link_libraries(fiaisdk_wallet PUBLIC fiaisdk_core_application fiaisdk_crypto_core
    PRIVATE fiaisdk_compiler_options)
