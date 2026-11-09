---
paths:
  - "CMakeLists.txt"
  - "**/CMakeLists.txt"
  - "cmake/**"
  - "CMakePresets.json"
---
# CMake rules

- Target-based only: `target_sources/include_directories/link_libraries/compile_features`.
  No `include_directories()`, `add_definitions()`, or global `CMAKE_CXX_FLAGS` edits.
- Core targets: `fiaisdk_core_domain` ← `fiaisdk_core_application` ← `fiaisdk_core_infrastructure`.
- Per module (once it has layers): `fiaisdk_<m>_core` (domain+application, links only
  `fiaisdk_core_application`) and `fiaisdk_<m>_infra` (links `_core`, `fiaisdk_core_infrastructure`,
  3rd-party). The shared `fiaisdk` links them all.
- GoogleTest only via `cmake/GoogleTest.cmake` and only in `tests/` (ADR-0004).
- Every internal target links `fiaisdk_compiler_options` (warnings) — never add warnings ad hoc.
- Options prefixed `FIAISDK_`. Compiler-specific flags only inside `if(MSVC)`/generator expressions.
- Third-party deps: one mechanism project-wide (decide in ADR: vcpkg manifest or FetchContent).
- Install must export `FiaiSDK::fiaisdk` + `fiaisdkConfig.cmake` + `ConfigVersion` for `find_package`.
- New presets must exist for all three OSes when they are platform-relevant.
