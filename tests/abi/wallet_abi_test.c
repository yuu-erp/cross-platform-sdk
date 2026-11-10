#include "test_check.h"
#include <fiaisdk/fiaisdk.h>
#include <stddef.h>
#include <string.h>

int main(void) {
    fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    fiaisdk_wallet *wallet = NULL, *random_wallet = NULL;
    uint8_t key[32] = {0}, digest[32] = {0}, signature[96], valid = 99;
    fiaisdk_wallet_info info = FIAISDK_WALLET_INFO_INIT;
    CHECK(sizeof(info) == 160 && offsetof(fiaisdk_wallet_info, address) == 117);
    CHECK(fiaisdk_sdk_create(&config, &sdk) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_wallet_import(sdk, key, 32, key, 32, &wallet) ==
              FIAISDK_STATUS_INVALID_ARGUMENT &&
          wallet == NULL);
    key[31] = 1;
    CHECK(fiaisdk_wallet_import(sdk, key, 32, key, 32, &wallet) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_wallet_get_info(wallet, &info) == FIAISDK_STATUS_OK);
    CHECK(info.address[0] == 0x7E && info.secp256k1_public_key[0] == 4 &&
          info.bls_public_key[0] == 0x97);
    CHECK(info.reserved0[0] == 0 && info.struct_size == 160);
    {
        struct future_info {
            fiaisdk_wallet_info known;
            uint8_t tail[8];
        } future;
        memset(&future, 0xAA, sizeof(future));
        future.known.struct_size = (uint32_t)sizeof(future);
        CHECK(fiaisdk_wallet_get_info(wallet, &future.known) == FIAISDK_STATUS_OK);
        CHECK(future.tail[0] == 0xAA && future.tail[7] == 0xAA);
        CHECK(future.known.struct_size == sizeof(future));
    }
    info.struct_size = 4;
    CHECK(fiaisdk_wallet_get_info(wallet, &info) == FIAISDK_STATUS_INVALID_ARGUMENT);
    info.struct_size = 160;
    CHECK(fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_BLS12381, NULL, 0, signature,
                              96) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_crypto_verify(sdk, FIAISDK_CRYPTO_SCHEME_BLS12381, info.bls_public_key, 48, NULL,
                                0, signature, 96, &valid) == FIAISDK_STATUS_OK &&
          valid == 1);
    CHECK(fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_SECP256K1, digest, 32, signature,
                              65) == FIAISDK_STATUS_OK);
    CHECK(fiaisdk_crypto_verify(sdk, FIAISDK_CRYPTO_SCHEME_SECP256K1, info.secp256k1_public_key, 65,
                                digest, 32, signature, 65, &valid) == FIAISDK_STATUS_OK &&
          valid == 1);
    signature[64] ^= 1;
    CHECK(fiaisdk_crypto_verify(sdk, FIAISDK_CRYPTO_SCHEME_SECP256K1, info.secp256k1_public_key, 65,
                                digest, 32, signature, 65, &valid) == FIAISDK_STATUS_OK &&
          valid == 0);
    CHECK(fiaisdk_crypto_verify(sdk, 999, info.bls_public_key, 48, NULL, 0, signature, 96,
                                &valid) == FIAISDK_STATUS_INVALID_ARGUMENT &&
          valid == 0);
    CHECK(fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_BLS12381, NULL, 0, signature,
                              95) == FIAISDK_STATUS_BUFFER_TOO_SMALL);
    CHECK(fiaisdk_wallet_sign(sdk, wallet, FIAISDK_CRYPTO_SCHEME_SECP256K1, digest, 31, signature,
                              65) == FIAISDK_STATUS_INVALID_ARGUMENT);
    {
        uint8_t zero[65] = {0};
        CHECK(memcmp(signature, zero, 65) == 0);
    }
    CHECK(fiaisdk_wallet_create(sdk, &random_wallet) == FIAISDK_STATUS_OK);
    fiaisdk_wallet_destroy(random_wallet);
    CHECK(fiaisdk_wallet_create(sdk, NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_wallet_create(NULL, &random_wallet) == FIAISDK_STATUS_INVALID_ARGUMENT &&
          random_wallet == NULL);
    CHECK(fiaisdk_wallet_import(sdk, NULL, 32, key, 32, &random_wallet) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_wallet_import(sdk, key, 31, key, 32, &random_wallet) ==
          FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_wallet_get_info(wallet, NULL) == FIAISDK_STATUS_INVALID_ARGUMENT);
    CHECK(fiaisdk_wallet_get_info(NULL, &info) == FIAISDK_STATUS_INVALID_ARGUMENT);
    {
        uint8_t zero[48] = {0};
        CHECK(memcmp(info.bls_public_key, zero, 48) == 0);
    }
    fiaisdk_sdk_destroy(sdk);
    CHECK(fiaisdk_wallet_get_info(wallet, &info) == FIAISDK_STATUS_OK);
    fiaisdk_wallet_destroy(wallet);
    fiaisdk_wallet_destroy(NULL);
    return 0;
}
