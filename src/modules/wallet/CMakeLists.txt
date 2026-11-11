add_library(fiaisdk_wallet STATIC
    api/wallet_module.cpp
    application/import_wallet.cpp)
target_link_libraries(fiaisdk_wallet PUBLIC fiaisdk_core_application fiaisdk_crypto_core
    PRIVATE fiaisdk_compiler_options)

include(${PROJECT_SOURCE_DIR}/cmake/SQLiteDependency.cmake)

add_library(fiaisdk_wallet_infra STATIC infrastructure/sqlite_wallet_store.cpp)
target_link_libraries(fiaisdk_wallet_infra PUBLIC fiaisdk_wallet
    PRIVATE fiaisdk_compiler_options fiaisdk_sqlite)
