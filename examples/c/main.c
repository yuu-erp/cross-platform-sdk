/* FiaiSDK C example: create the SDK and use the network module.
 *
 *   fiaisdk_example_c [endpoint-url]
 *
 * Pass a listening TCP endpoint such as tcp://127.0.0.1:9545 to exercise the network.
 */
#include <fiaisdk/fiaisdk.h>

#include <stdio.h>
#include <stdlib.h>

static int fail(const char *what, fiaisdk_status status) {
    fprintf(stderr, "%s failed: %s (%d)\n", what, fiaisdk_status_message(status), (int)status);
    return 1;
}

int main(int argc, char **argv) {
    const char *url = argc > 1 ? argv[1] : NULL;

    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    config.application_name = "c-example";
    config.application_version = "1.0.0";

    fiaisdk_sdk *sdk = NULL;
    fiaisdk_status status = fiaisdk_sdk_create(&config, &sdk);
    if (status != FIAISDK_STATUS_OK) {
        return fail("fiaisdk_sdk_create", status);
    }
    printf("FiaiSDK %s\n", fiaisdk_version());
    {
        const uint8_t message[] = {'a', 'b', 'c'};
        uint8_t digest[FIAISDK_CRYPTO_SHA256_SIZE];
        status = fiaisdk_crypto_sha256(sdk, message, sizeof(message), digest, sizeof(digest));
        if (status != FIAISDK_STATUS_OK) {
            fiaisdk_sdk_destroy(sdk);
            return fail("fiaisdk_crypto_sha256", status);
        }
        printf("SHA-256 of demo message calculated (%u bytes)\n", (unsigned)sizeof(digest));
    }

    {
        const uint8_t message[] = {'a', 'b', 'c'};
        uint8_t digest[32], signature[96], valid = 0;
        fiaisdk_wallet *wallet = NULL;
        fiaisdk_wallet_info info = FIAISDK_WALLET_INFO_INIT;
        status = fiaisdk_wallet_create(sdk, &wallet);
        if (status == FIAISDK_STATUS_OK)
            status = fiaisdk_wallet_get_info(wallet, &info);
        if (status == FIAISDK_STATUS_OK)
            status = fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_BLS12381, message,
                                         sizeof(message), signature, sizeof(signature));
        if (status == FIAISDK_STATUS_OK)
            status = fiaisdk_crypto_verify(sdk, FIAISDK_CRYPTO_SCHEME_BLS12381, info.bls_public_key,
                                           sizeof(info.bls_public_key), message, sizeof(message),
                                           signature, 96, &valid);
        if (status == FIAISDK_STATUS_OK && valid == 1)
            status =
                fiaisdk_crypto_keccak256(sdk, message, sizeof(message), digest, sizeof(digest));
        else if (status == FIAISDK_STATUS_OK)
            status = FIAISDK_STATUS_INTERNAL_ERROR;
        if (status == FIAISDK_STATUS_OK)
            status = fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_SECP256K1, digest,
                                         sizeof(digest), signature, sizeof(signature));
        if (status == FIAISDK_STATUS_OK)
            status = fiaisdk_crypto_verify(
                sdk, FIAISDK_CRYPTO_SCHEME_SECP256K1, info.secp256k1_public_key,
                sizeof(info.secp256k1_public_key), digest, sizeof(digest), signature, 65, &valid);
        fiaisdk_wallet_destroy(wallet);
        if (status != FIAISDK_STATUS_OK || valid != 1) {
            fiaisdk_sdk_destroy(sdk);
            return fail("offline wallet",
                        status == FIAISDK_STATUS_OK ? FIAISDK_STATUS_INTERNAL_ERROR : status);
        }
        printf("Offline BLS/secp256k1 signatures verified\n");
    }

    if (url == NULL) {
        fiaisdk_sdk_destroy(sdk);
        return 0;
    }
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
