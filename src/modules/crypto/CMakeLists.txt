include(${PROJECT_SOURCE_DIR}/cmake/CryptoDependencies.cmake)

add_library(fiaisdk_crypto_core STATIC api/crypto_module.cpp)
target_link_libraries(fiaisdk_crypto_core PUBLIC fiaisdk_core_application
    PRIVATE fiaisdk_compiler_options)

add_library(fiaisdk_crypto_infra STATIC infrastructure/library_crypto_provider.cpp)
target_link_libraries(fiaisdk_crypto_infra PUBLIC fiaisdk_crypto_core
    PRIVATE fiaisdk_compiler_options OpenSSL::Crypto secp256k1 fiaisdk_blst)
