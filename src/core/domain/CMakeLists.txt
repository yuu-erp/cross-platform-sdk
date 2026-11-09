# Core domain: building blocks shared by every module's domain layer.
# Depends on the C++ standard library only (no I/O, no OS, no third-party).
add_library(fiaisdk_core_domain STATIC
    secure_bytes.cpp
)
target_include_directories(fiaisdk_core_domain PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(fiaisdk_core_domain PUBLIC cxx_std_20)
target_link_libraries(fiaisdk_core_domain PRIVATE fiaisdk_compiler_options)
