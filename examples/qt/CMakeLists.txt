cmake_minimum_required(VERSION 3.25)
project(FiaiSdkQtExample LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_add_executable(fiaisdk_example_qt main.cpp)
target_compile_features(fiaisdk_example_qt PRIVATE cxx_std_17)
target_include_directories(fiaisdk_example_qt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(fiaisdk_example_qt PRIVATE Qt6::Widgets Qt6::Concurrent)
set_target_properties(fiaisdk_example_qt PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

set(FIAISDK_LIBRARY "" CACHE FILEPATH "Path to a built FiaiSDK shared library")
set(_library_name "${CMAKE_SHARED_LIBRARY_PREFIX}fiaisdk${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(_staged_library "${CMAKE_CURRENT_BINARY_DIR}/sdk/${_library_name}")
set(_sdk_library "${FIAISDK_LIBRARY}")
if(_sdk_library)
    get_filename_component(_sdk_library "${_sdk_library}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    if(NOT EXISTS "${_sdk_library}" OR IS_DIRECTORY "${_sdk_library}")
        message(FATAL_ERROR "FIAISDK_LIBRARY must point to a built shared library: ${_sdk_library}")
    endif()
else()
    unset(_sdk_library)
    find_file(_sdk_library NAMES "${_library_name}"
        PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/../../build/release/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../build/debug/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../build/msvc/bin/Release"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../build/msvc/bin/Debug"
        NO_DEFAULT_PATH NO_CACHE)
endif()

if(_sdk_library AND NOT _sdk_library STREQUAL _staged_library)
    add_custom_target(fiaisdk_example_qt_library
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sdk"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_sdk_library}" "${_staged_library}"
        COMMENT "Copying FiaiSDK shared library into the Qt build directory"
        VERBATIM)
    add_dependencies(fiaisdk_example_qt fiaisdk_example_qt_library)
elseif(NOT _sdk_library)
    message(STATUS "FiaiSDK library not found. Build it and reconfigure, set FIAISDK_LIBRARY, or use Browse at runtime.")
endif()

target_compile_definitions(fiaisdk_example_qt PRIVATE
    FIAISDK_STAGED_LIBRARY_PATH="${_staged_library}")
