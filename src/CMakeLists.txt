# Internal targets (never installed, never exported):
#   fiaisdk_core_domain          STATIC  Result/Error, StrongType, DomainEvent, SecureBytes
#   fiaisdk_core_application     STATIC  ports (IClock, ILogger, IEventBus), Module, lifecycle
#   fiaisdk_core_infrastructure  STATIC  default adapters (SystemClock, NullLogger, event bus)
#   fiaisdk_<module>             STATIC  one per module (later split per layer)
# Public target:
#   fiaisdk                      SHARED  C ABI (bindings) + composition root, links everything.
# Include root for all internal code is src/ ("core/domain/result.h", "modules/wallet/...").

add_subdirectory(core)
add_subdirectory(modules/network)
add_subdirectory(modules/wallet)
add_subdirectory(modules/smc)

# Bindings and the composition root are compiled directly into the shared library:
#  - bindings: exported symbols in a static lib can be dropped by the linker (Windows).
#  - composition root: depends on every module, keeping the core targets free of module deps.
add_library(fiaisdk SHARED
    bindings/c_api/network_c_api.cpp
    bindings/c_api/sdk_c_api.cpp
    bindings/c_api/status_mapping.cpp
    core/composition_root.cpp
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
        fiaisdk_network_infra
        fiaisdk_wallet
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
