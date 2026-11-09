/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_SDK_H
#define FIAISDK_SDK_H

#include <fiaisdk/export.h>
#include <fiaisdk/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Version of the loaded library as "MAJOR.MINOR.PATCH".
 * Returns a pointer to static storage: never NULL, never free it. Thread-safe.
 * Compare with FIAISDK_VERSION_STRING to detect header/binary mismatch.
 */
FIAISDK_API const char *FIAISDK_CALL fiaisdk_version(void);

/**
 * Version of the loaded library encoded as FIAISDK_MAKE_VERSION(major, minor, patch).
 * Thread-safe.
 */
FIAISDK_API uint32_t FIAISDK_CALL fiaisdk_version_number(void);

/**
 * Human-readable English description of a status code.
 * Returns static storage: never NULL, never free it. Unknown codes return "Unknown status".
 * Thread-safe.
 */
FIAISDK_API const char *FIAISDK_CALL fiaisdk_status_message(fiaisdk_status status);

/**
 * Create and initialise an SDK instance with all built-in modules.
 *
 * @param config   Required. See fiaisdk_sdk_config for struct_size rules.
 * @param out_sdk  Required. Receives the new instance on success, NULL on failure.
 *                 Release it with fiaisdk_sdk_destroy.
 * @return FIAISDK_STATUS_OK, FIAISDK_STATUS_INVALID_ARGUMENT (NULL argument or
 *         struct_size < FIAISDK_SDK_CONFIG_V1_SIZE), FIAISDK_STATUS_OUT_OF_MEMORY,
 *         FIAISDK_STATUS_INTERNAL_ERROR (a module failed to initialise).
 *
 * Thread-safety: may be called concurrently; each call creates an independent instance.
 * Blocking: runs module initialisation synchronously on the calling thread.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_sdk_create(const fiaisdk_sdk_config *config,
                                                           fiaisdk_sdk **out_sdk);

/**
 * Shut down all modules (reverse initialisation order) and free the instance.
 * NULL is accepted and ignored. The handle must not be used afterwards.
 *
 * Thread-safety: must not run concurrently with any other call on the same instance.
 */
FIAISDK_API void FIAISDK_CALL fiaisdk_sdk_destroy(fiaisdk_sdk *sdk);

/**
 * Query whether a module is registered in this instance.
 *
 * @param sdk            Required.
 * @param module_name    Required. UTF-8, NUL-terminated, e.g. "wallet". Case-sensitive.
 * @param out_available  Required. Set to 1 if present, 0 otherwise (also 0 on error).
 * @return FIAISDK_STATUS_OK or FIAISDK_STATUS_INVALID_ARGUMENT.
 *
 * Thread-safety: safe to call concurrently on the same instance.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_sdk_has_module(const fiaisdk_sdk *sdk,
                                                               const char *module_name,
                                                               uint8_t *out_available);

#ifdef __cplusplus
}
#endif

#endif /* FIAISDK_SDK_H */
