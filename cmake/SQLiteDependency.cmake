# ADR-0010: official, pinned SQLite amalgamation. No SQLite runtime dependency.
include(FetchContent)
FetchContent_Declare(fiaisdk_sqlite_source
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH SHA256=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(fiaisdk_sqlite_source)

find_package(Threads REQUIRED)
add_library(fiaisdk_sqlite STATIC "${fiaisdk_sqlite_source_SOURCE_DIR}/sqlite3.c")
target_include_directories(fiaisdk_sqlite SYSTEM PUBLIC "${fiaisdk_sqlite_source_SOURCE_DIR}")
target_compile_definitions(fiaisdk_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_USE_URI=0)
target_link_libraries(fiaisdk_sqlite PRIVATE Threads::Threads)
