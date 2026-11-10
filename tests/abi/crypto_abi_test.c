#include "test_check.h"
#include <fiaisdk/fiaisdk.h>

#include <string.h>

int main(void) {
    const uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                  0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                  0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    const uint8_t input[] = {'a', 'b', 'c'};
    uint8_t output[33];
    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    uint8_t present = 0;
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_sdk_has_module(sdk, "crypto", &present) == FIAISDK_STATUS_OK && present == 1);
    memset(output, 0xAA, sizeof(output));
    CHECK(fiaisdk_crypto_sha256(sdk, input, sizeof(input), output, sizeof(output)) ==
          FIAISDK_STATUS_OK);
    CHECK(memcmp(output, expected, 32) == 0 && output[32] == 0xAA);
    CHECK(fiaisdk_crypto_sha256(sdk, input, sizeof(input), output, 31) ==
          FIAISDK_STATUS_BUFFER_TOO_SMALL);
    CHECK(memcmp(output, expected, 32) == 0);
    CHECK(fiaisdk_crypto_sha256(sdk, NULL, 0, output, 32) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_crypto_sha256(sdk, NULL, 1, output, 32) == FIAISDK_STATUS_INVALID_ARGUMENT);
    {
        uint8_t zero[32] = {0};
        CHECK(memcmp(output, zero, 32) == 0);
    }
    CHECK(fiaisdk_crypto_sha256(sdk, input, 3, NULL, 32) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_crypto_sha256(NULL, input, 3, output, 32) == FIAISDK_STATUS_INVALID_ARGUMENT);
    memcpy(output, input, 3);
    CHECK(fiaisdk_crypto_sha256(sdk, output, 3, output, 32) == FIAISDK_STATUS_OK);
    CHECK(memcmp(output, expected, 32) == 0);
    CHECK(fiaisdk_crypto_random_bytes(sdk, output, sizeof(output)) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_crypto_random_bytes(sdk, NULL, 1) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_crypto_random_bytes(sdk, output, 0) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_crypto_random_bytes(sdk, output, FIAISDK_CRYPTO_MAX_RANDOM_SIZE + 1) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_crypto_random_bytes(NULL, output, sizeof(output)) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    {
        uint8_t zero[33] = {0};
        CHECK(memcmp(output, zero, 33) == 0);
    }
    fiaisdk_sdk_destroy(sdk);
    return 0;
}
