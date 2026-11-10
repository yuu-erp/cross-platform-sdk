include(FetchContent)
# Keep the upstream default from introducing a global shared-library cache option.
set(BUILD_SHARED_LIBS OFF)

set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_CTIME_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SECP256K1_INSTALL OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_RECOVERY ON CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_ECDH OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_EXTRAKEYS OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_SCHNORRSIG OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_MUSIG OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_ELLSWIFT OFF CACHE BOOL "" FORCE)
set(SECP256K1_DISABLE_SHARED ON CACHE BOOL "" FORCE)
FetchContent_Declare(secp256k1
    GIT_REPOSITORY https://github.com/bitcoin-core/secp256k1.git
    GIT_TAG a660a4976efe880bae7982ee410b9e0dc59ac983
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_Declare(blst
    GIT_REPOSITORY https://github.com/supranational/blst.git
    GIT_TAG e7f90de551e8df682f3cc99067d204d8b90d27ad
    SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(secp256k1 blst)
set(FIAISDK_BLST_SOURCE_DIR ${blst_SOURCE_DIR} CACHE INTERNAL "Pinned blst source path")
set(FIAISDK_SECP256K1_SOURCE_DIR ${secp256k1_SOURCE_DIR} CACHE INTERNAL "Pinned secp256k1 source path")

# Use upstream assembly on supported 64-bit targets; portable mode dispatches CPU features.
if(MSVC)
    if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        message(FATAL_ERROR "This blst build integration currently supports Windows x64; ARM64 needs armasm64 integration")
    endif()
    enable_language(ASM_MASM)
    file(GLOB _blst_asm CONFIGURE_DEPENDS "${blst_SOURCE_DIR}/build/win64/*-x86_64.asm")
else()
    enable_language(ASM)
    set(_blst_asm ${blst_SOURCE_DIR}/build/assembly.S)
endif()
add_library(fiaisdk_blst STATIC ${blst_SOURCE_DIR}/src/server.c ${_blst_asm})
target_compile_definitions(fiaisdk_blst PRIVATE __BLST_PORTABLE__)
if(MSVC)
    # Preserve case of upstream C symbols, as in blst's official build.bat.
    target_compile_options(fiaisdk_blst PRIVATE
        $<$<COMPILE_LANGUAGE:ASM_MASM>:/Cp> $<$<COMPILE_LANGUAGE:ASM_MASM>:/Cx>)
endif()
target_include_directories(fiaisdk_blst SYSTEM PUBLIC ${blst_SOURCE_DIR}/bindings)
set_target_properties(fiaisdk_blst PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
