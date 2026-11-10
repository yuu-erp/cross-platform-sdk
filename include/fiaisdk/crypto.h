/* SPDX-License-Identifier: UNLICENSED */
#ifndef FIAISDK_CRYPTO_H
#define FIAISDK_CRYPTO_H

#include <fiaisdk/export.h>
#include <fiaisdk/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIAISDK_CRYPTO_SHA256_SIZE 32
#define FIAISDK_CRYPTO_MAX_RANDOM_SIZE 65536

/**
 * SHA-256 of binary input (not Keccak-256 or SHA3-256).
 *
 * sdk is required and borrowed. input is borrowed for this call; NULL is allowed only when
 * input_size is zero. output is caller-owned, with output_size writable bytes; at least 32
 * bytes are required. Only the first 32 bytes are written. Input and output may overlap.
 * On failure, a non-NULL output with capacity >= 32 has its first 32 bytes cleared;
 * smaller output buffers are untouched. No allocation or free crosses the ABI.
 *
 * Returns OK, INVALID_ARGUMENT (invalid pointer/length), BUFFER_TOO_SMALL (capacity < 32),
 * MODULE_NOT_FOUND, UNAVAILABLE (provider failure), OUT_OF_MEMORY or INTERNAL_ERROR.
 * Thread-safe, synchronous; time depends on input_size. Must not race SDK destruction.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_sha256(const fiaisdk_sdk *sdk,
                                                              const uint8_t *input,
                                                              size_t input_size, uint8_t *output,
                                                              size_t output_size);

/**
 * Fill caller-owned output with cryptographically secure private random bytes.
 *
 * sdk and output are required and borrowed for this call. output_size is both the requested
 * byte count and writable capacity, in [1, 65536]. On failure, a non-NULL buffer with a valid
 * output_size is entirely cleared. Invalid sizes leave the buffer untouched. The SDK retains
 * no copy. The caller must protect and wipe output if it is used as a secret.
 *
 * Returns OK, INVALID_ARGUMENT, MODULE_NOT_FOUND, UNAVAILABLE (entropy/provider failure),
 * OUT_OF_MEMORY or INTERNAL_ERROR. Never falls back to a non-cryptographic random source.
 * Thread-safe, synchronous; may block while the provider obtains entropy.
 * Must not race SDK destruction.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_random_bytes(const fiaisdk_sdk *sdk,
                                                                    uint8_t *output,
                                                                    size_t output_size);

/** Key schemes are int32_t values; never pass a C enum type across the ABI. */
enum { FIAISDK_CRYPTO_SCHEME_BLS12381 = 1, FIAISDK_CRYPTO_SCHEME_SECP256K1 = 2 };

/** Keccak-256, with exactly the ownership, buffer, error and threading contract of sha256. */
FIAISDK_API fiaisdk_status FIAISDK_CALL fiaisdk_crypto_keccak256(const fiaisdk_sdk *sdk,
                                                                 const uint8_t *input,
                                                                 size_t input_size, uint8_t *output,
                                                                 size_t output_size);

/**
 * Verify BLS12-381 or recoverable secp256k1 signature without owning secret keys.
 * All pointers are borrowed for the call. sdk, public_key, signature and out_valid are required;
 * data may be NULL only when empty. out_valid must not overlap any input. It is set to 0 on
 * all errors, 1 for a valid signature. Correct-length invalid material returns OK with 0.
 * BLS: compressed G1 key (48), compressed G2 signature (96), raw message of any size including
 * empty, DST BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_POP_. No aggregate/PoP API is provided.
 * secp256k1: uncompressed key (65, 0x04 prefix), exactly 32 digest bytes, low-S signature
 * r||s||recovery-id (65, recovery-id 0..3). Recovery ID must match the supplied public key.
 * Wrong schemes/lengths/pointers return INVALID_ARGUMENT. Other statuses: MODULE_NOT_FOUND,
 * UNAVAILABLE, OUT_OF_MEMORY, INTERNAL_ERROR. Thread-safe, synchronous; must not race SDK
 * destruction. Caller owns buffers; nothing is retained or freed by the SDK.
 */
FIAISDK_API fiaisdk_status FIAISDK_CALL
fiaisdk_crypto_verify(const fiaisdk_sdk *sdk, int32_t scheme, const uint8_t *public_key,
                      size_t public_key_size, const uint8_t *data, size_t data_size,
                      const uint8_t *signature, size_t signature_size, uint8_t *out_valid);

#ifdef __cplusplus
}
#endif

#endif /* FIAISDK_CRYPTO_H */
