/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_NETWORK_H
#define FIAISDK_NETWORK_H

#include <fiaisdk/export.h>
#include <fiaisdk/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Network module: the SDK's connection to one node.
 *
 * DEMO STATUS: the transport is simulated (no network I/O) until the HTTP/WebSocket library
 * ADR lands. Hosts ending in ".invalid" behave as unreachable; any other valid host connects
 * with a deterministic latency of 5..49 ms. The API below is meant to stay stable.
 *
 * Endpoint rules: https:// or wss:// (http:// and ws:// only for exactly localhost, 127.0.0.1
 * or [::1]); no "user:password@" in the URL; ASCII host; port 1..65535; at most 2048 characters.
 *
 * Common to every function below:
 * - must not run concurrently with fiaisdk_sdk_destroy on the same instance;
 * - may also return FIAISDK_STATUS_MODULE_NOT_FOUND, FIAISDK_STATUS_OUT_OF_MEMORY or
 *   FIAISDK_STATUS_INTERNAL_ERROR.
 */

/** Values of fiaisdk_network_status.state. */
enum { FIAISDK_NETWORK_STATE_DISCONNECTED = 0, FIAISDK_NETWORK_STATE_CONNECTED = 1 };

/**
 * Connection status, filled by fiaisdk_network_get_status.
 * Extensible like fiaisdk_sdk_config: set struct_size (use FIAISDK_NETWORK_STATUS_INIT); the
 * library writes only the fields that fit in the caller's struct_size.
 */
typedef struct fiaisdk_network_status {
    uint32_t struct_size;
    /** FIAISDK_NETWORK_STATE_* */
    int32_t state;
    /** When the connection was established, milliseconds since 1970-01-01 UTC; 0 if none. */
    int64_t connected_at_unix_ms;
    /** Round-trip time measured when connecting; 0 if disconnected. */
    uint32_t latency_ms;
    /** Always 0. Keeps the layout identical on 32- and 64-bit platforms. */
    uint32_t reserved0;
} fiaisdk_network_status;

#define FIAISDK_NETWORK_STATUS_V1_SIZE                                                             \
    FIAISDK_U32_(offsetof(fiaisdk_network_status, reserved0) + sizeof(uint32_t))

#define FIAISDK_NETWORK_STATUS_INIT                                                                \
    { FIAISDK_U32_(sizeof(fiaisdk_network_status)), 0, 0, 0, 0 }

/**
 * Connect to the node at `endpoint_url`, replacing the current connection if the new one
 * succeeds. Connecting again to the same endpoint is a no-op that returns OK.
 *
 * @param sdk           Required.
 * @param endpoint_url  Required. UTF-8, NUL-terminated. Copied.
 * @param timeout_ms    1..60000.
 * @return FIAISDK_STATUS_OK; FIAISDK_STATUS_INVALID_ARGUMENT (NULL argument, malformed or
 *         insecure URL, timeout out of range); FIAISDK_STATUS_UNAVAILABLE (node unreachable,
 *         no answer in time, or a concurrent connect/disconnect did not finish in time — the
 *         previous connection, if any, is kept).
 *
 * Blocking: at most about timeout_ms in total on the calling thread, including any wait for a
 * concurrent connect/disconnect (do not call from a UI thread with large timeouts).
 * Thread-safety: safe; concurrent connect/disconnect calls are serialised.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_connect(fiaisdk_sdk *sdk,
                                                                const char *endpoint_url,
                                                                uint32_t timeout_ms);

/**
 * Close the current connection. Returns OK when already disconnected.
 * Blocking: waits for a connect in progress (bounded by that connect's timeout_ms).
 * Thread-safety: safe.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_disconnect(fiaisdk_sdk *sdk);

/**
 * Read the connection status.
 *
 * @param sdk            Required.
 * @param inout_status   Required. struct_size must be >= FIAISDK_NETWORK_STATUS_V1_SIZE.
 * @return FIAISDK_STATUS_OK or FIAISDK_STATUS_INVALID_ARGUMENT.
 *
 * Never blocks on a connect in progress. Thread-safety: safe.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_network_get_status(const fiaisdk_sdk *sdk, fiaisdk_network_status *inout_status);

/**
 * Copy the normalised URL of the current endpoint (lower-case scheme/host, default port
 * omitted, path/query kept) into `buffer`. Empty string when disconnected.
 *
 * Size protocol: *inout_size is the capacity of `buffer` in bytes on input (buffer may be NULL
 * when it is 0). On output it is the required size including the terminating NUL when the
 * result is FIAISDK_STATUS_OK or FIAISDK_STATUS_BUFFER_TOO_SMALL, and 0 on any other error.
 * If the capacity is too small, FIAISDK_STATUS_BUFFER_TOO_SMALL is returned and `buffer` is not
 * modified: allocate *inout_size bytes and call again.
 *
 * The URL may contain secrets (API keys in the path/query): do not log it.
 * Thread-safety: safe.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_network_get_endpoint(const fiaisdk_sdk *sdk,
                                                                     char *buffer,
                                                                     size_t *inout_size);

#ifdef __cplusplus
}
#endif

#endif /* FIAISDK_NETWORK_H */
