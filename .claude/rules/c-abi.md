---
paths:
  - "include/fiaisdk/**"
  - "src/bindings/**"
---
# Public C ABI rules

The C ABI is the compatibility contract. Treat every edit here as a release-note item.

Headers (`include/fiaisdk/*.h`)
- Must compile as C99 and C++: `extern "C"` guards, `<stdint.h>`, `<stddef.h>` only.
- Every function: `FIAISDK_API <ret> FIAISDK_CALL fiaisdk_<module>_<verb>(...)`.
- Public macros contain no casts (C++ consumers may build with `-Wold-style-cast`).
- Return `fiaisdk_status` (`int32_t`). Outputs via trailing `out_` pointers, set to a safe value
  (NULL/0) first on every path.
- Types: fixed-width ints, `const char*` UTF-8 NUL-terminated, opaque handles
  (`typedef struct fiaisdk_wallet fiaisdk_wallet;`). No `bool` (use `uint8_t`), no C `enum` type
  in signatures or structs (use `int32_t` + `#define`/constants), no STL, no C++ classes.
- Extensible structs: first field `uint32_t struct_size`; append-only; accept
  `struct_size >= FIAISDK_<NAME>_V1_SIZE`; zero-fill unknown tail internally.
- Document in the header comment for each function: ownership of every pointer, who frees,
  thread-safety, whether it blocks, callback thread + lifetime of `user_data`.
- Never change the value of an existing status code or the signature of an existing function.
  Deprecate with `FIAISDK_DEPRECATED` and add `_v2`.

Implementation (`src/bindings/c_api/*.cpp`)
- Every exported function body: validate args → `try { ... } catch (...) { return map(...); }`.
  No exception, `abort`, or `assert` reaches the caller on bad input.
- Translate handle → internal facade; map `Result` errors with the single `to_status()` mapper.
- Strings out: caller buffer + `size_t* inout_len` (return `FIAISDK_STATUS_BUFFER_TOO_SMALL`
  with required length) or SDK-owned string freed by `fiaisdk_string_free`.
- Destroy functions accept NULL.
- Add an ABI test in `tests/abi/` (compiled as C) and update `docs/api/`.
