/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_TYPES_H
#define FIAISDK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Result of every fallible SDK call.
 *
 * Passed as int32_t (never as a C enum type) so its size is fixed on every compiler.
 * Values are part of the ABI: existing values never change, new ones are only appended.
 */
typedef int32_t fiaisdk_status;

enum {
    FIAISDK_STATUS_OK = 0,
    FIAISDK_STATUS_INVALID_ARGUMENT = 1,
    FIAISDK_STATUS_NOT_INITIALIZED = 2,
    FIAISDK_STATUS_MODULE_NOT_FOUND = 3,
    FIAISDK_STATUS_BUFFER_TOO_SMALL = 4,
    FIAISDK_STATUS_OUT_OF_MEMORY = 5,
    FIAISDK_STATUS_UNAVAILABLE = 6, /* resource unreachable, locked or timed out */
    FIAISDK_STATUS_INTERNAL_ERROR = 100,
    FIAISDK_STATUS_NOT_FOUND = 101,
    FIAISDK_STATUS_ALREADY_EXISTS = 102
};

/** Opaque SDK instance. Created by fiaisdk_sdk_create, released by fiaisdk_sdk_destroy. */
typedef struct fiaisdk_sdk fiaisdk_sdk;

/**
 * Configuration for fiaisdk_sdk_create.
 *
 * Extensible: `struct_size` must be set to sizeof(fiaisdk_sdk_config) as seen by the caller.
 * New fields are only appended; the library accepts any struct_size >=
 * FIAISDK_SDK_CONFIG_V1_SIZE and treats fields beyond the caller's size as zero/NULL.
 * The SDK copies every string during the call; the caller keeps ownership.
 */
typedef struct fiaisdk_sdk_config {
    uint32_t struct_size;
    /** UTF-8, NUL-terminated, optional (NULL = empty). */
    const char *application_name;
    /** UTF-8, NUL-terminated, optional (NULL = empty). */
    const char *application_version;
} fiaisdk_sdk_config;

/* Casts in public macros: C-style in C, static_cast in C++ (consumers may use -Wold-style-cast),
 * so MSVC (C4267) and GCC/Clang (-Wconversion) see an explicit uint32_t. */
#ifdef __cplusplus
#define FIAISDK_U32_(x) static_cast<uint32_t>(x)
#else
#define FIAISDK_U32_(x) ((uint32_t)(x))
#endif

/** Size of the first published layout of fiaisdk_sdk_config. Never changes. */
#define FIAISDK_SDK_CONFIG_V1_SIZE                                                                 \
    FIAISDK_U32_(offsetof(fiaisdk_sdk_config, application_version) + sizeof(const char *))

/** Zero-initialised config with struct_size set: `fiaisdk_sdk_config c = FIAISDK_SDK_CONFIG_INIT;`
 */
#define FIAISDK_SDK_CONFIG_INIT {FIAISDK_U32_(sizeof(fiaisdk_sdk_config)), NULL, NULL}

#ifdef __cplusplus
}
#endif

#endif /* FIAISDK_TYPES_H */
