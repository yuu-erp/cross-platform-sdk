# Internal targets (never installed, never exported):
#   fiaisdk_core_domain          STATIC  Result/Error, StrongType, DomainEvent, SecureBytes
#   fiaisdk_core_application     STATIC  ports (IClock, ILogger, IEventBus), Module, lifecycle
#   fiaisdk_core_infrastructure  STATIC  default adapters (SystemClock, NullLogger, event bus)
#   fiaisdk_crypto_infra         STATIC  crypto adapters (links fiaisdk_crypto_core)
#   fiaisdk_network_infra        STATIC  network adapters (links fiaisdk_network_core)
#   fiaisdk_wallet               STATIC  offline wallet
#   fiaisdk_smc                  STATIC  SMC module
# Public target:
#   fiaisdk                      SHARED  C ABI (bindings) + bootstrap/composition_root.cpp, links everything.
# Include root for all internal code is src/ ("core/domain/result.h", "modules/wallet/...").

# Private dependency of the crypto provider (ADR-0006, ADR-0007). The wallet store reaches
# AES-256-GCM through the crypto facade (ADR-0010).
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL 3.2...<4 REQUIRED COMPONENTS Crypto)

add_subdirectory(core)
add_subdirectory(modules/crypto)
add_subdirectory(modules/network)
add_subdirectory(modules/wallet)
add_subdirectory(modules/smc)

# Bindings and the composition root are compiled directly into the shared library:
#  - bindings: exported symbols in a static lib can be dropped by the linker (Windows).
#  - composition root: depends on every module, keeping the core targets free of module deps.
add_library(fiaisdk SHARED
    bindings/c_api/crypto_c_api.cpp
    bindings/c_api/wallet_c_api.cpp
    bindings/c_api/wallet_store_c_api.cpp
    bindings/c_api/network_c_api.cpp
    bindings/c_api/sdk_c_api.cpp
    bindings/c_api/status_mapping.cpp
    bootstrap/composition_root.cpp
)
add_library(FiaiSDK::fiaisdk ALIAS fiaisdk)

target_include_directories(fiaisdk
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${FIAISDK_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(fiaisdk PRIVATE FIAISDK_BUILDING_LIBRARY)
# The header-only C++ wrapper (include/fiaisdk/cpp) needs C++17; C consumers are unaffected.
target_compile_features(fiaisdk INTERFACE cxx_std_17)
target_link_libraries(fiaisdk
    PRIVATE
        fiaisdk_compiler_options
        fiaisdk_core_infrastructure
        fiaisdk_crypto_infra
        fiaisdk_network_infra
        fiaisdk_wallet_infra
        fiaisdk_smc
)

# Visibility presets hide our code, but std:: headers force default visibility on some symbols
# (e.g. typeinfo of std exceptions). Linker export lists guarantee only fiaisdk_* is exported.
if(APPLE)
    set(_fiaisdk_exports ${PROJECT_SOURCE_DIR}/cmake/fiaisdk.exp)
    target_link_options(fiaisdk PRIVATE "LINKER:-exported_symbols_list,${_fiaisdk_exports}")
    set_property(TARGET fiaisdk APPEND PROPERTY LINK_DEPENDS ${_fiaisdk_exports})
elseif(UNIX)
    set(_fiaisdk_exports ${PROJECT_SOURCE_DIR}/cmake/fiaisdk.map)
    target_link_options(fiaisdk PRIVATE "LINKER:--version-script=${_fiaisdk_exports}")
    set_property(TARGET fiaisdk APPEND PROPERTY LINK_DEPENDS ${_fiaisdk_exports})
endif()

if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(_fiaisdk_soversion ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
else()
    set(_fiaisdk_soversion ${PROJECT_VERSION_MAJOR})
endif()
set_target_properties(fiaisdk PROPERTIES
    OUTPUT_NAME fiaisdk
    VERSION ${PROJECT_VERSION}
    SOVERSION ${_fiaisdk_soversion}
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
