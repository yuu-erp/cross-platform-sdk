/* ABI contract tests for include/fiaisdk/sdk.h. Compiled as C99 on purpose. */
#include <fiaisdk/fiaisdk.h>

#include "test_check.h"

#include <stddef.h>
#include <string.h>

static void test_version(void) {
    CHECK(fiaisdk_version() != NULL);
    CHECK(strcmp(fiaisdk_version(), FIAISDK_VERSION_STRING) == 0);
    CHECK(fiaisdk_version_number() == FIAISDK_VERSION_NUMBER);
}

static void test_status_codes_are_stable(void) {
    /* Values are part of the ABI and must never change. */
    CHECK(FIAISDK_STATUS_OK == 0);
    CHECK(FIAISDK_STATUS_INVALID_ARGUMENT == 1);
    CHECK(FIAISDK_STATUS_NOT_INITIALIZED == 2);
    CHECK(FIAISDK_STATUS_MODULE_NOT_FOUND == 3);
    CHECK(FIAISDK_STATUS_BUFFER_TOO_SMALL == 4);
    CHECK(FIAISDK_STATUS_OUT_OF_MEMORY == 5);
    CHECK(FIAISDK_STATUS_INTERNAL_ERROR == 100);
    CHECK(sizeof(fiaisdk_status) == 4);
}

static void test_status_message_covers_every_code(void) {
    const fiaisdk_status codes[] = {FIAISDK_STATUS_OK,
                                    FIAISDK_STATUS_INVALID_ARGUMENT,
                                    FIAISDK_STATUS_NOT_INITIALIZED,
                                    FIAISDK_STATUS_MODULE_NOT_FOUND,
                                    FIAISDK_STATUS_BUFFER_TOO_SMALL,
                                    FIAISDK_STATUS_OUT_OF_MEMORY,
                                    FIAISDK_STATUS_INTERNAL_ERROR};
    size_t i;
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i) {
        const char *msg = fiaisdk_status_message(codes[i]);
        CHECK(msg != NULL);
        CHECK(strcmp(msg, "Unknown status") != 0);
    }
    CHECK(strcmp(fiaisdk_status_message(-1), "Unknown status") == 0);
    CHECK(strcmp(fiaisdk_status_message(9999), "Unknown status") == 0);
}

static void test_config_v1_layout_is_frozen(void) {
    /* Published V1 layout. If any of these change, the ABI is broken. */
    const size_t ptr = sizeof(void *);
    CHECK(offsetof(fiaisdk_sdk_config, struct_size) == 0);
    CHECK(offsetof(fiaisdk_sdk_config, application_name) == (ptr == 8 ? 8U : 4U));
    CHECK(offsetof(fiaisdk_sdk_config, application_version) == (ptr == 8 ? 16U : 8U));
    CHECK(FIAISDK_SDK_CONFIG_V1_SIZE == (ptr == 8 ? 24U : 12U));
    CHECK(FIAISDK_SDK_CONFIG_V1_SIZE <= sizeof(fiaisdk_sdk_config));
}

static void test_create_accepts_exact_v1_size(void) {
    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    config.struct_size = (uint32_t)FIAISDK_SDK_CONFIG_V1_SIZE;
    config.application_name = "v1-caller";
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
    CHECK(sdk != NULL);
    fiaisdk_sdk_destroy(sdk);
}

static void test_create_rejects_invalid_arguments(void) {
    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = (fiaisdk_sdk *)&config; /* non-NULL garbage: must be reset to NULL */

    CHECK(fiaisdk_sdk_create(&config, NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);

    CHECK(fiaisdk_sdk_create(NULL, &sdk) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(sdk == NULL);

    config.struct_size = (uint32_t)(FIAISDK_SDK_CONFIG_V1_SIZE - 1U);
    sdk = (fiaisdk_sdk *)&config;
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(sdk == NULL);

    config.struct_size = 0;
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_INVALID_ARGUMENT);
}

static void test_create_with_null_strings(void) {
    const fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
    CHECK(sdk != NULL);
    fiaisdk_sdk_destroy(sdk);
}

static void test_create_accepts_newer_larger_struct(void) {
    /* Simulates a caller compiled against a future header with extra trailing fields. */
    struct future_config {
        fiaisdk_sdk_config v1;
        const char *future_field;
        uint64_t another_future_field;
    } future;
    fiaisdk_sdk *sdk = NULL;

    memset(&future, 0, sizeof(future));
    future.v1.struct_size = (uint32_t)sizeof(future);
    future.v1.application_name = "abi-test";
    future.v1.application_version = "1.0.0";
    future.future_field = "ignored";

    CHECK(fiaisdk_sdk_create(&future.v1, &sdk) == FIAISDK_STATUS_OK);
    CHECK(sdk != NULL);
    fiaisdk_sdk_destroy(sdk);
}

static void test_has_module(void) {
    const fiaisdk_sdk_config config = {(uint32_t)sizeof(fiaisdk_sdk_config), "abi-test", "1.0.0"};
    fiaisdk_sdk *sdk = NULL;
    uint8_t available = 7;

    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);

    CHECK(fiaisdk_sdk_has_module(sdk, "wallet", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 1);
    CHECK(fiaisdk_sdk_has_module(sdk, "smc", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 1);
    CHECK(fiaisdk_sdk_has_module(sdk, "network", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 1);

    CHECK(fiaisdk_sdk_has_module(sdk, "does_not_exist", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 0);
    CHECK(fiaisdk_sdk_has_module(sdk, "", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 0);
    CHECK(fiaisdk_sdk_has_module(sdk, "Wallet", &available) == FIAISDK_STATUS_OK);
    CHECK(available == 0);

    available = 7;
    CHECK(fiaisdk_sdk_has_module(sdk, NULL, &available) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(available == 0);
    available = 7;
    CHECK(fiaisdk_sdk_has_module(NULL, "wallet", &available) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(available == 0);
    CHECK(fiaisdk_sdk_has_module(sdk, "wallet", NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);

    fiaisdk_sdk_destroy(sdk);
}

static void test_destroy_null_is_noop(void) {
    fiaisdk_sdk_destroy(NULL);
}

static void test_repeated_lifecycle(void) {
    const fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    int i;
    for (i = 0; i < 100; ++i) {
        fiaisdk_sdk *sdk = NULL;
        CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
        fiaisdk_sdk_destroy(sdk);
    }
}

int main(void) {
    test_version();
    test_status_codes_are_stable();
    test_status_message_covers_every_code();
    test_config_v1_layout_is_frozen();
    test_create_accepts_exact_v1_size();
    test_create_rejects_invalid_arguments();
    test_create_with_null_strings();
    test_create_accepts_newer_larger_struct();
    test_has_module();
    test_destroy_null_is_noop();
    test_repeated_lifecycle();
    return TEST_RESULT();
}
