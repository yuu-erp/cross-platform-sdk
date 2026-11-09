# src/core is layered like every module (ADR-0005). Each layer is its own target, linked in the
# direction infrastructure -> application -> domain. Header includes are checked by
# tools/check_layering.py (all internal targets share the src/ include root).
add_subdirectory(domain)         # fiaisdk_core_domain
add_subdirectory(application)    # fiaisdk_core_application
add_subdirectory(infrastructure) # fiaisdk_core_infrastructure
# composition_root.cpp is compiled into the fiaisdk shared library (see src/CMakeLists.txt).
