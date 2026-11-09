/* FiaiSDK C example: create the SDK and use the network module.
 *
 *   fiaisdk_example_c [endpoint-url]
 *
 * Default endpoint: https://node.example.com/rpc. Try "https://down.invalid" to see an
 * unreachable node, or "http://node.example.com" to see the TLS policy reject it.
 * NOTE: the network transport is simulated in this version (no real I/O).
 */
#include <fiaisdk/fiaisdk.h>

#include <stdio.h>
#include <stdlib.h>

static int fail(const char *what, fiaisdk_status status) {
    fprintf(stderr, "%s failed: %s (%d)\n", what, fiaisdk_status_message(status), (int)status);
    return 1;
}

int main(int argc, char **argv) {
    const char *url = argc > 1 ? argv[1] : "https://node.example.com/rpc";

    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    config.application_name = "c-example";
    config.application_version = "1.0.0";

    fiaisdk_sdk *sdk = NULL;
    fiaisdk_status status = fiaisdk_sdk_create(&config, &sdk);
    if (status != FIAISDK_STATUS_OK) {
        return fail("fiaisdk_sdk_create", status);
    }
    printf("FiaiSDK %s\n", fiaisdk_version());

    status = fiaisdk_network_connect(sdk, url, 5000);
    if (status != FIAISDK_STATUS_OK) {
        fiaisdk_sdk_destroy(sdk);
        return fail("fiaisdk_network_connect", status);
    }

    fiaisdk_network_status net = FIAISDK_NETWORK_STATUS_INIT;
    status = fiaisdk_network_get_status(sdk, &net);
    if (status != FIAISDK_STATUS_OK) {
        fiaisdk_sdk_destroy(sdk);
        return fail("fiaisdk_network_get_status", status);
    }

    /* Two-call size protocol: ask for the size, allocate, read. */
    size_t size = 0;
    fiaisdk_network_get_endpoint(sdk, NULL, &size);
    char *endpoint = (char *)malloc(size);
    if (endpoint != NULL &&
        fiaisdk_network_get_endpoint(sdk, endpoint, &size) == FIAISDK_STATUS_OK) {
        /* Demo only: a real app must not print/log the full URL (may contain an API key). */
        printf("connected: %s\n", endpoint);
    }
    free(endpoint);
    printf("latency: %u ms, connected at (unix ms): %lld\n", (unsigned)net.latency_ms,
           (long long)net.connected_at_unix_ms);

    fiaisdk_network_disconnect(sdk);
    fiaisdk_sdk_destroy(sdk);
    printf("disconnected\n");
    return 0;
}
