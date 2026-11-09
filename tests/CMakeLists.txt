# Test framework not chosen yet (pending ADR); tests use tests/support/test_check.h.

function(fiaisdk_add_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
    target_link_libraries(${name} PRIVATE fiaisdk_compiler_options ${T_LIBS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Public C ABI, compiled as strict C99.
fiaisdk_add_test(fiaisdk_abi_test SOURCES abi/sdk_abi_test.c LIBS FiaiSDK::fiaisdk)
set_target_properties(fiaisdk_abi_test PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# Header-only C++ wrapper, as a consumer uses it.
# Built as C++17: the minimum standard the wrapper promises (propagated by FiaiSDK::fiaisdk).
find_package(Threads REQUIRED)
fiaisdk_add_test(fiaisdk_cpp_wrapper_test SOURCES cpp/sdk_wrapper_test.cpp
                 LIBS FiaiSDK::fiaisdk Threads::Threads)
set_target_properties(fiaisdk_cpp_wrapper_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# Internal unit tests link internal static targets directly.
fiaisdk_add_test(fiaisdk_kernel_result_test SOURCES unit/kernel/result_test.cpp LIBS fiaisdk_kernel)
fiaisdk_add_test(fiaisdk_core_module_registry_test SOURCES unit/core/module_registry_test.cpp LIBS fiaisdk_core)

# Exactly the symbols in abi/expected_exports.txt may be exported.
if(WIN32)
    find_program(FIAISDK_EXPORT_TOOL NAMES dumpbin)
    set(_fiaisdk_export_kind pe)
elseif(APPLE)
    find_program(FIAISDK_EXPORT_TOOL NAMES nm llvm-nm)
    set(_fiaisdk_export_kind macho)
else()
    find_program(FIAISDK_EXPORT_TOOL NAMES nm llvm-nm)
    set(_fiaisdk_export_kind elf)
endif()
if(FIAISDK_EXPORT_TOOL)
    add_test(NAME fiaisdk_exported_symbols
        COMMAND ${CMAKE_COMMAND} -DTOOL=${FIAISDK_EXPORT_TOOL} -DKIND=${_fiaisdk_export_kind}
                -DLIB=$<TARGET_FILE:fiaisdk>
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/abi/expected_exports.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/abi/check_exports.cmake)
else()
    message(WARNING "No nm/dumpbin found: exported-symbol check disabled")
endif()

if(FIAISDK_BUILD_EXAMPLES)
    add_test(NAME fiaisdk_example_c_smoke COMMAND fiaisdk_example_c)
    add_test(NAME fiaisdk_example_cpp_smoke COMMAND fiaisdk_example_cpp)
endif()
