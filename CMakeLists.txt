cmake_minimum_required(VERSION 3.25)

project(FiaiSDK
    VERSION 0.1.0
    DESCRIPTION "Modular cross-platform SDK for the Meta-Node ecosystem"
    LANGUAGES C CXX
)

option(FIAISDK_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(FIAISDK_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(FIAISDK_ENABLE_INSTALL "Generate install and package rules" ${PROJECT_IS_TOP_LEVEL})
option(FIAISDK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FIAISDK_ENABLE_SANITIZERS "Build with sanitizers (GCC/Clang), see FIAISDK_SANITIZERS" OFF)
set(FIAISDK_SANITIZERS "address,undefined" CACHE STRING
    "-fsanitize= list when FIAISDK_ENABLE_SANITIZERS is ON (e.g. address,undefined or thread)")

foreach(_part MAJOR MINOR PATCH)
    if(PROJECT_VERSION_${_part} GREATER 255)
        message(FATAL_ERROR "FIAISDK_MAKE_VERSION encodes each version part in 8 bits (0..255)")
    endif()
endforeach()

include(cmake/BuildOptions.cmake)
include(cmake/CompilerOptions.cmake)

# Generated public header: include/fiaisdk/version.h (in the build tree).
set(FIAISDK_GENERATED_INCLUDE_DIR ${PROJECT_BINARY_DIR}/generated/include)
configure_file(cmake/version.h.in ${FIAISDK_GENERATED_INCLUDE_DIR}/fiaisdk/version.h @ONLY)

add_subdirectory(src)

if(FIAISDK_BUILD_EXAMPLES)
    add_subdirectory(examples/c)
    add_subdirectory(examples/cpp)
endif()

if(FIAISDK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FIAISDK_ENABLE_INSTALL)
    include(cmake/InstallConfig.cmake)
endif()
