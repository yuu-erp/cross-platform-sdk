/* ABI contract tests for include/fiaisdk/network.h. Compiled as C99 on purpose. */
#include <fiaisdk/fiaisdk.h>

#include "network/loopback_listener.h"
#include "test_check.h"

#include <stddef.h>
#include <string.h>

static fiaisdk_sdk *create_sdk(void) {
    const fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
    return sdk;
}

static void test_layout_is_frozen(void) {
    CHECK(FIAISDK_STATUS_UNAVAILABLE == 6);
    CHECK(FIAISDK_NETWORK_STATE_DISCONNECTED == 0);
    CHECK(FIAISDK_NETWORK_STATE_CONNECTED == 1);
    CHECK(offsetof(fiaisdk_network_status, struct_size) == 0);
    CHECK(offsetof(fiaisdk_network_status, state) == 4);
    CHECK(offsetof(fiaisdk_network_status, connected_at_unix_ms) == 8);
    CHECK(offsetof(fiaisdk_network_status, latency_ms) == 16);
    CHECK(offsetof(fiaisdk_network_status, reserved0) == 20);
    CHECK(FIAISDK_NETWORK_STATUS_V1_SIZE == 24);
    CHECK(sizeof(fiaisdk_network_status) == 24);
}

static void test_rejects_null_arguments(void) {
    fiaisdk_sdk *sdk = create_sdk();
    fiaisdk_network_status status = FIAISDK_NETWORK_STATUS_INIT;
    size_t size = 0;
    char buffer[4];

    CHECK(fiaisdk_network_connect(NULL, "https://node.example.com", 1000) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, NULL, 1000) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_disconnect(NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_get_status(NULL, &status) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_get_status(sdk, NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_get_endpoint(sdk, buffer, NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);
    size = sizeof(buffer);
    CHECK(fiaisdk_network_get_endpoint(sdk, NULL, &size) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(size == 0); /* error paths report 0 */
    size = sizeof(buffer);
    CHECK(fiaisdk_network_get_endpoint(NULL, buffer, &size) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(size == 0);

    fiaisdk_sdk_destroy(sdk);
}

static void test_connect_validation(void) {
    fiaisdk_sdk *sdk = create_sdk();
    CHECK(fiaisdk_network_connect(sdk, "not a url", 1000) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "http://node.example.com", 1000) ==
          FIAISDK_STATUS_INVALID_ARGUMENT); /* plain http to a remote host */
    CHECK(fiaisdk_network_connect(sdk, "https://node.example.com", 0) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "https://node.example.com", 60001) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "https://node.example.com", 1000) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "tcp://localhost:9545", 1000) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "tcp://127.0.0.1", 1000) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_network_connect(sdk, "tcp://127.0.0.1:9545/rpc", 1000) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    fiaisdk_sdk_destroy(sdk);
}

static void test_status_lifecycle(const char *url) {
    fiaisdk_sdk *sdk = create_sdk();
    fiaisdk_network_status status = FIAISDK_NETWORK_STATUS_INIT;

    CHECK(fiaisdk_network_get_status(sdk, &status) == FIAISDK_STATUS_OK);
    CHECK(status.state == FIAISDK_NETWORK_STATE_DISCONNECTED);
    CHECK(status.latency_ms == 0);
    CHECK(status.connected_at_unix_ms == 0);

    CHECK(fiaisdk_network_connect(sdk, url, 5000) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_network_get_status(sdk, &status) == FIAISDK_STATUS_OK);
    CHECK(status.state == FIAISDK_NETWORK_STATE_CONNECTED);
    CHECK(status.latency_ms <= 5000);
    CHECK(status.connected_at_unix_ms > 1600000000000LL);        /* after Sep 2020 */
    CHECK(status.struct_size == sizeof(fiaisdk_network_status)); /* untouched */

    CHECK(fiaisdk_network_disconnect(sdk) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_network_get_status(sdk, &status) == FIAISDK_STATUS_OK);
    CHECK(status.state == FIAISDK_NETWORK_STATE_DISCONNECTED);
    CHECK(fiaisdk_network_disconnect(sdk) == FIAISDK_STATUS_OK); /* idempotent */

    fiaisdk_sdk_destroy(sdk);
}

static void test_status_struct_size_rules(const char *url) {
    fiaisdk_sdk *sdk = create_sdk();
    fiaisdk_network_status status = FIAISDK_NETWORK_STATUS_INIT;
    struct future_status {
        fiaisdk_network_status v1;
        uint64_t future_field;
    } future;

    status.struct_size = FIAISDK_NETWORK_STATUS_V1_SIZE - 1U;
    CHECK(fiaisdk_network_get_status(sdk, &status) == FIAISDK_STATUS_INVALID_ARGUMENT);

    CHECK(fiaisdk_network_connect(sdk, url, 5000) == FIAISDK_STATUS_OK);
    memset(&future, 0xAB, sizeof(future));
    future.v1.struct_size = (uint32_t)sizeof(future);
    CHECK(fiaisdk_network_get_status(sdk, &future.v1) == FIAISDK_STATUS_OK);
    CHECK(future.v1.state == FIAISDK_NETWORK_STATE_CONNECTED);
    CHECK(future.v1.reserved0 == 0);
    CHECK(future.future_field == 0xABABABABABABABABULL); /* unknown tail left untouched */

    fiaisdk_sdk_destroy(sdk);
}

static void test_get_endpoint_size_protocol(const char *url) {
    fiaisdk_sdk *sdk = create_sdk();
    const char *expected = url;
    char small[8];
    char exact[64];
    size_t size = 0;

    /* Disconnected: empty string, required size 1. */
    CHECK(fiaisdk_network_get_endpoint(sdk, NULL, &size) == FIAISDK_STATUS_BUFFER_TOO_SMALL);
    CHECK(size == 1);
    size = sizeof(exact);
    CHECK(fiaisdk_network_get_endpoint(sdk, exact, &size) == FIAISDK_STATUS_OK);
    CHECK(exact[0] == '\0');

    CHECK(fiaisdk_network_connect(sdk, url, 5000) == FIAISDK_STATUS_OK);

    size = 0;
    CHECK(fiaisdk_network_get_endpoint(sdk, NULL, &size) == FIAISDK_STATUS_BUFFER_TOO_SMALL);
    CHECK(size == strlen(expected) + 1);

    memcpy(small, "keep", 5);
    size = sizeof(small);
    CHECK(fiaisdk_network_get_endpoint(sdk, small, &size) == FIAISDK_STATUS_BUFFER_TOO_SMALL);
    CHECK(size == strlen(expected) + 1);
    CHECK(strcmp(small, "keep") == 0); /* buffer not modified */

    size = strlen(expected) + 1; /* exactly enough */
    CHECK(fiaisdk_network_get_endpoint(sdk, exact, &size) == FIAISDK_STATUS_OK);
    CHECK(strcmp(exact, expected) == 0);

    fiaisdk_sdk_destroy(sdk);
}

static void test_destroy_while_connected(const char *url) {
    fiaisdk_sdk *sdk = create_sdk();
    CHECK(fiaisdk_network_connect(sdk, url, 5000) == FIAISDK_STATUS_OK);
    fiaisdk_sdk_destroy(sdk); /* must shut the module down cleanly (checked by ASan) */
}

int main(void) {
    test_layout_is_frozen();
    test_rejects_null_arguments();
    test_connect_validation();
    const loopback_listener listener = test_open_loopback(1);
    CHECK(listener.socket != TEST_BAD_SOCKET);
    if (listener.socket == TEST_BAD_SOCKET) {
        return TEST_RESULT();
    }
    test_status_lifecycle(listener.url);
    test_status_struct_size_rules(listener.url);
    test_get_endpoint_size_protocol(listener.url);
    test_destroy_while_connected(listener.url);
    test_close_loopback(listener);
    return TEST_RESULT();
}
