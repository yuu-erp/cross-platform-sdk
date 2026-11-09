#include <fiaisdk/fiaisdk.h>

#include <stdio.h>

int main(void) {
    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    config.application_name = "c-example";
    config.application_version = "1.0.0";

    fiaisdk_sdk *sdk = NULL;
    fiaisdk_status status = fiaisdk_sdk_create(&config, &sdk);
    if (status != FIAISDK_STATUS_OK) {
        fprintf(stderr, "SDK init failed: %s\n", fiaisdk_status_message(status));
        return 1;
    }

    uint8_t has_wallet = 0;
    status = fiaisdk_sdk_has_module(sdk, "wallet", &has_wallet);
    if (status != FIAISDK_STATUS_OK) {
        fprintf(stderr, "has_module failed: %s\n", fiaisdk_status_message(status));
        fiaisdk_sdk_destroy(sdk);
        return 1;
    }

    printf("FiaiSDK %s | wallet available: %s\n", fiaisdk_version(), has_wallet ? "yes" : "no");

    fiaisdk_sdk_destroy(sdk);
    return 0;
}
