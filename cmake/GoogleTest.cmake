# GoogleTest + GoogleMock for tests only (ADR-0004).
# 1) find_package(GTest 1.17) if a system/vcpkg/Homebrew copy exists, else
# 2) FetchContent of the pinned commit. Offline builds can point
#    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path to googletest checkout>.

include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723 # v1.17.0
    SYSTEM # third-party headers: no warnings from our -Werror flags
    FIND_PACKAGE_ARGS 1.17 NAMES GTest
)

# Use the same CRT as the rest of the project on MSVC and never install gtest with the SDK.
# Plain variables (CMP0077 NEW): they configure googletest's options without touching the cache.
set(gtest_force_shared_crt ON)
set(INSTALL_GTEST OFF)
set(BUILD_GMOCK ON)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)
