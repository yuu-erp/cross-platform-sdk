# Standalone consumer project: verifies the installed package works via find_package.
cmake_minimum_required(VERSION 3.24)
project(FiaiSDKPackageTest LANGUAGES C CXX)

find_package(FiaiSDK 0.1 REQUIRED)

add_executable(fiaisdk_package_test main.cpp)
target_link_libraries(fiaisdk_package_test PRIVATE FiaiSDK::fiaisdk)
# No compile feature on purpose: FiaiSDK::fiaisdk must propagate the C++17 requirement.
