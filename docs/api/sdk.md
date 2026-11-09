# SDK lifecycle API (`fiaisdk/sdk.h`)

All functions are `extern "C"`, use the `FIAISDK_CALL` calling convention and never throw.

| Function | Purpose | Thread-safety |
|---|---|---|
| `const char* fiaisdk_version(void)` | Runtime library version `"MAJOR.MINOR.PATCH"` (static storage) | any thread |
| `uint32_t fiaisdk_version_number(void)` | Same, as `FIAISDK_MAKE_VERSION(ma, mi, pa)` | any thread |
| `const char* fiaisdk_status_message(fiaisdk_status)` | English text for a status; `"Unknown status"` for unknown codes | any thread |
| `fiaisdk_status fiaisdk_sdk_create(const fiaisdk_sdk_config*, fiaisdk_sdk**)` | Create + initialise all modules | concurrent calls OK (independent instances) |
| `void fiaisdk_sdk_destroy(fiaisdk_sdk*)` | Shut down modules (reverse order) and free; `NULL` ignored | not concurrent with other calls on the same handle |
| `fiaisdk_status fiaisdk_sdk_has_module(const fiaisdk_sdk*, const char*, uint8_t*)` | Is module registered? Case-sensitive | concurrent calls OK |

## Status codes (`fiaisdk/types.h`)

`fiaisdk_status` is `int32_t`. Values never change; new values are appended.

| Code | Value | Meaning |
|---|---|---|
| `FIAISDK_STATUS_OK` | 0 | success |
| `FIAISDK_STATUS_INVALID_ARGUMENT` | 1 | NULL required argument, bad `struct_size`, moved-from C++ object |
| `FIAISDK_STATUS_NOT_INITIALIZED` | 2 | reserved |
| `FIAISDK_STATUS_MODULE_NOT_FOUND` | 3 | reserved for module-specific calls |
| `FIAISDK_STATUS_BUFFER_TOO_SMALL` | 4 | reserved for caller-buffer outputs |
| `FIAISDK_STATUS_OUT_OF_MEMORY` | 5 | allocation failed inside the SDK |
| `FIAISDK_STATUS_INTERNAL_ERROR` | 100 | module failed to initialise / unexpected failure |

## `fiaisdk_sdk_config`

```c
typedef struct fiaisdk_sdk_config {
    uint32_t struct_size;            /* = sizeof(fiaisdk_sdk_config); use FIAISDK_SDK_CONFIG_INIT */
    const char* application_name;    /* optional, UTF-8, copied */
    const char* application_version; /* optional, UTF-8, copied */
} fiaisdk_sdk_config;
```

Compatibility: the library accepts any `struct_size >= FIAISDK_SDK_CONFIG_V1_SIZE`. Older callers
(smaller struct) get defaults for fields they do not know; newer callers (larger struct) have
unknown trailing fields ignored.

## Version check

```c
if (fiaisdk_version_number() / 256U != FIAISDK_VERSION_NUMBER / 256U) {
    /* runtime MAJOR.MINOR differs from the headers you compiled against */
}
```

## C++ wrapper (`fiaisdk/cpp/sdk.hpp`)

- `fiaisdk::Sdk(const Config& = {})` — RAII, move-only; throws `fiaisdk::Error` (has `status()`).
- `bool Sdk::hasModule(std::string_view) const` — throws `Error(INVALID_ARGUMENT)` on a moved-from object.
- `fiaisdk_sdk* Sdk::native() const` — borrow the C handle.
- `std::string_view fiaisdk::runtimeVersion()`.
