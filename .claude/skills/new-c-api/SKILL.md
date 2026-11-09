---
name: new-c-api
description: Add or change a public C ABI function/struct in include/fiaisdk and its binding in src/bindings/c_api, plus the C++ wrapper, ABI test and docs. Use whenever a public SDK function, struct, handle, status code or callback is added or modified.
---
# New / changed C API

Follow `.claude/rules/c-abi.md` strictly.

1. Write the header declaration first, with a doc comment covering: purpose, each param
   (in/out, nullable?), ownership + who frees, thread-safety, blocking?, status codes returned.
2. Check compatibility: is this additive? If an existing signature/struct/status value would
   change → stop, propose `_v2` + deprecation, and ask.
3. Implement in `src/bindings/c_api/<m>_c_api.cpp` (pattern: `sdk_c_api.cpp`). Add the source to the
   `fiaisdk` target in `src/CMakeLists.txt` (bindings are compiled into the shared lib, not a static lib):
   ```cpp
   FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_<m>_<verb>(fiaisdk_<m>* h, ..., T* out_x) {
       if (out_x) *out_x = {};
       if (!h || !out_x) return FIAISDK_STATUS_INVALID_ARGUMENT;
       try {
           auto r = h->facade.<verb>(...);
           if (!r) return fiaisdk::bindings::toStatus(r.error());
           *out_x = ...;
           return FIAISDK_STATUS_OK;
       } catch (...) {
           return fiaisdk::bindings::statusFromCurrentException();
       }
   }
   ```
4. Add the inline wrapper in `include/fiaisdk/cpp/<m>.hpp` (RAII handle, no exported symbols).
5. Tests: `tests/abi/` (C99: NULL args, buffer too small, struct_size older/newer) + unit test of the
   facade path. The `fiaisdk_exported_symbols` test verifies only `fiaisdk_*` is exported.
6. Update `docs/api/<m>.md` and an example in `examples/c` or `examples/cpp`.
7. Run `verify`, then the `abi-reviewer` subagent.
