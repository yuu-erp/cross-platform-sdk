/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_WALLET_H
#define FIAISDK_WALLET_H
#include <fiaisdk/crypto.h>
#ifdef __cplusplus
extern "C" {
#endif

/** Owning in-memory wallet; destroy with fiaisdk_wallet_destroy. No secret-export API. */
typedef struct fiaisdk_wallet fiaisdk_wallet;

/** Public metadata only; arrays contain binary bytes, not hex text. */
typedef struct fiaisdk_wallet_info {
    uint32_t struct_size;
    uint8_t bls_public_key[48];
    uint8_t secp256k1_public_key[65]; /* Uncompressed, includes 0x04. */
    uint8_t address[20];     /* Final 20 bytes of Keccak-256(secp public key excluding 0x04). */
    uint8_t bls_address[20]; /* Final 20 bytes of Keccak-256(compressed BLS public key). */
    uint8_t reserved0[3];    /* Always zero. */
} fiaisdk_wallet_info;
#define FIAISDK_WALLET_INFO_V1_SIZE 160
#define FIAISDK_WALLET_INFO_INIT                                                                   \
    {                                                                                              \
        FIAISDK_U32_(sizeof(fiaisdk_wallet_info)), {0}, {0}, {0}, {0}, {                           \
            0                                                                                      \
        }                                                                                          \
    }

/**
 * Create an offline wallet with independent BLS and secp256k1 private keys from a CSPRNG.
 * sdk and out_wallet are required, borrowed during the call. *out_wallet is NULL on failure.
 * Caller owns the returned handle and must destroy it. No persistence/mnemonic is provided.
 * Returns OK, INVALID_ARGUMENT, MODULE_NOT_FOUND, UNAVAILABLE, OUT_OF_MEMORY, INTERNAL_ERROR.
 * Thread-safe, synchronous; may block for entropy. Must not race SDK destruction.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_create(const fiaisdk_sdk *sdk,
                                                              fiaisdk_wallet **out_wallet);

/**
 * Import two explicitly supplied 32-byte big-endian private scalars. Both pointers are required.
 * The SDK copies keys for the wallet lifetime; caller remains responsible for wiping originals.
 * Zero/out-of-range keys are INVALID_ARGUMENT; no clamping/text parsing/seed derivation occurs.
 * To import a legacy shared-scalar pair, supply the same scalar for both parameters.
 * Ownership, errors, threading and blocking contract otherwise match fiaisdk_wallet_create.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_import(
    const fiaisdk_sdk *sdk, const uint8_t *bls_key, size_t bls_key_size,
    const uint8_t *secp256k1_key, size_t secp256k1_key_size, fiaisdk_wallet **out_wallet);

/**
 * Read metadata. wallet and inout_info are required, borrowed; caller owns inout_info.
 * struct_size must be >= V1_SIZE. Only known fields are written, preserving struct_size and
 * any future tail. On errors other than invalid/missing struct_size the known payload is zero.
 * Returns OK or INVALID_ARGUMENT. Thread-safe, synchronous, no I/O.
 * Must not race wallet destruction; no live SDK is needed.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_get_info(const fiaisdk_wallet *wallet,
                                                                fiaisdk_wallet_info *inout_info);

/**
 * Sign using FIAISDK_CRYPTO_SCHEME_BLS12381 (raw message bytes, 96-byte signature) or
 * FIAISDK_CRYPTO_SCHEME_SECP256K1 (exactly 32 digest bytes, 65-byte r||s||recovery-id).
 * No implicit hash/prefix is applied. BLS DST is documented in crypto.h/crypto API docs.
 * sdk, wallet, output are required, borrowed during this call. data may be NULL only if empty
 * (BLS only). output_size is caller-owned writable capacity, >= the signature size.
 * Only that signature-sized prefix is written; data/output may overlap. On failure a valid
 * output prefix is zeroed, except insufficient capacity/unknown scheme leave output untouched.
 * Returns OK, INVALID_ARGUMENT, BUFFER_TOO_SMALL, MODULE_NOT_FOUND, UNAVAILABLE,
 * OUT_OF_MEMORY or INTERNAL_ERROR. Thread-safe and synchronous. Must not race destruction
 * of sdk or wallet. Wallet may be signed using any live SDK with a compatible provider.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_wallet_sign(const fiaisdk_sdk *sdk,
                                                            const fiaisdk_wallet *wallet,
                                                            int32_t scheme, const uint8_t *data,
                                                            size_t data_size, uint8_t *output,
                                                            size_t output_size);

/** Wipe/free wallet. NULL is ignored; must not race calls using it. No live SDK needed. */
FIAISDK_API void FIAISDK_CALL fiaisdk_wallet_destroy(fiaisdk_wallet *wallet);

#ifdef __cplusplus
}
#endif
#endif /* FIAISDK_WALLET_H */
