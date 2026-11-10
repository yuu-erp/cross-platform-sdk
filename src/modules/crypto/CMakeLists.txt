# ADR-0006: a build-provisioned, maintained OpenSSL 3; no runtime headers leak to consumers.
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL 3.2...<4 REQUIRED COMPONENTS Crypto)
include(${PROJECT_SOURCE_DIR}/cmake/CryptoDependencies.cmake)

add_library(fiaisdk_crypto_core STATIC api/crypto_module.cpp)
target_link_libraries(fiaisdk_crypto_core PUBLIC fiaisdk_core_application
    PRIVATE fiaisdk_compiler_options)

add_library(fiaisdk_crypto_infra STATIC infrastructure/library_crypto_provider.cpp)
target_link_libraries(fiaisdk_crypto_infra PUBLIC fiaisdk_crypto_core
    PRIVATE fiaisdk_compiler_options OpenSSL::Crypto secp256k1 fiaisdk_blst)
