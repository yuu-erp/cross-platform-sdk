# C ABI guidelines

The authoritative rules for agents and reviewers are in `.claude/rules/c-abi.md`; summary:

- Export only `extern "C"` functions marked `FIAISDK_API ... FIAISDK_CALL`, named `fiaisdk_<module>_<verb>`.
- Opaque handles; fixed-width integers; `uint8_t` instead of `bool`; `int32_t` instead of enum types.
- Extensible structs start with `uint32_t struct_size`, are append-only, and accept older sizes.
- Every out-param is initialised on every path; destroy functions accept NULL.
- Document who allocates and frees every buffer, and callback thread/lifetime.
- Never let a C++ exception cross the boundary (`try { } catch (...)` + `statusFromCurrentException()`).
- Public macros contain no casts (C++ consumers may build with `-Wold-style-cast`).
- The export list is enforced at link time (`cmake/fiaisdk.map`, `cmake/fiaisdk.exp`) and by the
  `fiaisdk_exported_symbols` test, which compares against the exact list in
  `tests/abi/expected_exports.txt` (ELF via `nm`, Mach-O via `nm`, PE via `dumpbin`).
  **Every new public function is added to that file in the same PR.**
- Caller structs are read field by field and never beyond `struct_size` bytes
  (see `readConfig` in `src/bindings/c_api/sdk_c_api.cpp`); reuse that pattern for new structs.
- Pre-1.0: SONAME is `libfiaisdk.so.0.<minor>`; any minor bump may break ABI. From 1.0: `SOVERSION = MAJOR`.

## Open questions (decide by ADR before the first external release)

- Windows DLL name carries no ABI version pre-1.0 (`fiaisdk.dll` for every 0.x). Options:
  `fiaisdk-0.<minor>.dll`, or VERSIONINFO resource + documented side-by-side policy.
- ELF symbol versioning (`FIAISDK_0 { ... }` node in `cmake/fiaisdk.map`) is not used yet.
