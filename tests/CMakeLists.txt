# Test layout (ADR-0004):
#   abi/      plain C99 tests of the public C ABI (no framework, tests/support/test_check.h)
#   cpp/      GoogleTest tests of the header-only C++ wrapper, built as C++17
#   unit/     GoogleTest unit tests of internal code, mirroring src/ (unit/core/<layer>/...)
#   support/  shared test doubles (support/core: FakeClock, RecordingLogger, MockEventBus;
#             support/network: FakeNodeProbe)
#   package/  standalone find_package(FiaiSDK) consumer, built by CI after install

include(${PROJECT_SOURCE_DIR}/cmake/GoogleTest.cmake)
find_package(Threads REQUIRED)

# GoogleTest executable registered per test case with CTest.
function(fiaisdk_add_gtest name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE
        fiaisdk_compiler_options ${T_LIBS} GTest::gmock GTest::gtest_main Threads::Threads)
    # PRE_TEST: list tests when ctest runs, not at build time (no DLL lookup during the build).
    gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()

# --- Public C ABI, compiled as strict C99 ------------------------------------------------------
add_executable(fiaisdk_abi_test abi/sdk_abi_test.c)
target_include_directories(fiaisdk_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fiaisdk_abi_test PRIVATE fiaisdk_compiler_options FiaiSDK::fiaisdk)
set_target_properties(fiaisdk_abi_test PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
    # CHECK(...) on compile-time constants (ABI layout pins) is intentional: C4127.
    target_compile_options(fiaisdk_abi_test PRIVATE /wd4127)
endif()
add_test(NAME fiaisdk_abi_test COMMAND fiaisdk_abi_test)

add_executable(fiaisdk_network_abi_test abi/network_abi_test.c support/network/loopback_listener.c)
target_include_directories(fiaisdk_network_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fiaisdk_network_abi_test PRIVATE fiaisdk_compiler_options FiaiSDK::fiaisdk)
set_target_properties(fiaisdk_network_abi_test PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(fiaisdk_network_abi_test PRIVATE /wd4127)
endif()
add_test(NAME fiaisdk_network_abi_test COMMAND fiaisdk_network_abi_test)

# --- Header-only C++ wrapper, as a consumer uses it ---------------------------------------------
fiaisdk_add_gtest(fiaisdk_cpp_wrapper_tests
    SOURCES cpp/sdk_wrapper_test.cpp cpp/network_wrapper_test.cpp cpp/crypto_wrapper_test.cpp cpp/wallet_wrapper_test.cpp support/network/loopback_listener.c
    LIBS FiaiSDK::fiaisdk)
target_include_directories(fiaisdk_cpp_wrapper_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
if(WIN32)
    target_link_libraries(fiaisdk_network_abi_test PRIVATE ws2_32)
    target_link_libraries(fiaisdk_cpp_wrapper_tests PRIVATE ws2_32)
endif()
set_target_properties(fiaisdk_cpp_wrapper_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# --- Internal unit tests: src/core ---------------------------------------------------------------
fiaisdk_add_gtest(fiaisdk_core_tests
    SOURCES
        unit/core/domain/result_test.cpp
        unit/core/domain/secure_bytes_test.cpp
        unit/core/domain/strong_type_test.cpp
        unit/core/application/logger_test.cpp
        unit/core/application/module_registry_test.cpp
        unit/core/application/sdk_core_test.cpp
        unit/core/application/subscription_test.cpp
        unit/core/infrastructure/in_process_event_bus_test.cpp
        unit/core/infrastructure/system_clock_test.cpp
    LIBS fiaisdk_core_infrastructure)

# Allocation fault injection uses a separate executable with a replacement operator new.
fiaisdk_add_gtest(fiaisdk_event_bus_allocation_tests
    SOURCES unit/core/infrastructure/event_bus_allocation_test.cpp
    LIBS fiaisdk_core_infrastructure)

# --- Internal unit tests: src/modules/network --------------------------------------------------
fiaisdk_add_gtest(fiaisdk_network_tests
    SOURCES
        unit/network/domain/endpoint_test.cpp
        unit/network/application/connect_to_node_test.cpp
        unit/network/application/disconnect_from_node_test.cpp
        unit/network/infrastructure/simulated_node_probe_test.cpp
        ${PROJECT_SOURCE_DIR}/src/modules/network/infrastructure/simulated_node_probe.cpp
        unit/network/api/network_module_test.cpp
    LIBS fiaisdk_network_infra fiaisdk_core_infrastructure)

# --- Exactly the symbols in abi/expected_exports.txt may be exported -----------------------------
if(WIN32)
    # dumpbin is not on PATH outside a Developer prompt; it lives next to the MSVC linker.
    get_filename_component(_fiaisdk_linker_dir "${CMAKE_LINKER}" DIRECTORY)
    find_program(FIAISDK_EXPORT_TOOL NAMES dumpbin HINTS "${_fiaisdk_linker_dir}")
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
    # An invalid endpoint must make the example exit with an error.
    add_test(NAME fiaisdk_example_c_invalid_endpoint COMMAND fiaisdk_example_c tcp://127.0.0.1)
    set_tests_properties(fiaisdk_example_c_invalid_endpoint PROPERTIES WILL_FAIL TRUE)
endif()

# Crypto provider boundary and C99 surface.
fiaisdk_add_gtest(fiaisdk_crypto_tests
    SOURCES unit/crypto/crypto_test.cpp
    LIBS fiaisdk_crypto_infra fiaisdk_core_infrastructure FiaiSDK::fiaisdk)
add_executable(fiaisdk_crypto_abi_test abi/crypto_abi_test.c)
target_include_directories(fiaisdk_crypto_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fiaisdk_crypto_abi_test PRIVATE fiaisdk_compiler_options FiaiSDK::fiaisdk)
set_target_properties(fiaisdk_crypto_abi_test PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME fiaisdk_crypto_abi_test COMMAND fiaisdk_crypto_abi_test)

fiaisdk_add_gtest(fiaisdk_wallet_tests
    SOURCES unit/wallet/offline_wallet_test.cpp
    LIBS fiaisdk_wallet fiaisdk_crypto_infra FiaiSDK::fiaisdk fiaisdk_core_infrastructure)
add_executable(fiaisdk_wallet_abi_test abi/wallet_abi_test.c)
target_include_directories(fiaisdk_wallet_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(fiaisdk_wallet_abi_test PRIVATE fiaisdk_compiler_options FiaiSDK::fiaisdk)
set_target_properties(fiaisdk_wallet_abi_test PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
add_test(NAME fiaisdk_wallet_abi_test COMMAND fiaisdk_wallet_abi_test)

if(MSVC)
    target_compile_options(fiaisdk_wallet_abi_test PRIVATE /wd4127)
endif()
