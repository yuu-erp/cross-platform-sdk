# Offline wallet API

This slice is implemented from scratch using reviewed primitives. The owner requested checking
Archive to identify formats; no Archive code or seed-generation/storage logic is copied.
[ADR-0007](../adr/0007-offline-wallet-key-schemes.md) specifies the dual-key contract.

## Capabilities and scope

- Create independent BLS12-381 and secp256k1 private keys with CSPRNG-backed library operations.
- Import two explicit 32-byte big-endian private scalars. Zero/out-of-range values are rejected.
- Read public keys and two binary 20-byte addresses.
- Sign raw bytes using BLS or exactly 32 digest bytes using secp256k1.
- Verify via the [crypto API](crypto.md).

Keys exist only in memory and are wiped on destruction. This slice has no private-key export,
mnemonic, HD derivation, disk persistence, vault, backup, transaction encoding or node I/O.
An application that requires recoverable newly generated keys needs a separately designed
backup/vault capability before treating this slice as a persistent wallet product.

## Key/signature/address formats

| Scheme | Private input | Public key | Signature | Address |
|---|---|---|---|---|
| BLS12-381 | 32-byte big-endian nonzero scalar below r | Compressed G1, 48 bytes | Compressed G2, 96 bytes; raw message, DST in ADR-0007 | Last 20 bytes of Keccak-256(compressed G1 key) |
| secp256k1 | 32-byte big-endian nonzero scalar below n | Uncompressed 65 bytes starting 04 | Deterministic low-S r(32)||s(32)||recovery-id(1), recovery-id 0..3, signs a 32-byte digest | Last 20 bytes of Keccak-256(public key without 04) |

No automatic prefix/hash is applied by signing. The caller must select the intended scheme
and message/digest contract. No BLS aggregation/PoP API is provided. No EIP-191/EIP-155/RLP
transaction encoding is implied. Recoverable signatures with recovery ID 2/3 must be handled
according to the future transaction protocol rather than silently remapped.

New wallets use separate secrets; legacy pairs that used one scalar can be imported by
explicitly passing the same 32 bytes twice. The legacy word-list/hash/clamping derivation is
intentionally absent. Hex parsing is the caller's responsibility; raw input is never silently
interpreted as UTF-8 text or reduced modulo a curve order.

## C99 surface

Include `<fiaisdk/wallet.h>` or umbrella `<fiaisdk/fiaisdk.h>`.

| Function | Contract |
|---|---|
| `fiaisdk_wallet_create(sdk, &wallet)` | Synchronous CSPRNG/library generation; may block for entropy. Output NULL on failure. Caller owns handle. |
| `fiaisdk_wallet_import(sdk, bls, bls_size, secp, secp_size, &wallet)` | Copies two validated scalar buffers, each exactly 32 bytes. Caller must wipe originals. Output NULL on failure. |
| `fiaisdk_wallet_get_info(wallet, &info)` | Caller sets struct_size >= 160 (use INIT). Known fields written, struct_size/future tail preserved. Invalid wallet zeroes the known payload; invalid size leaves payload untouched. |
| `fiaisdk_wallet_sign(sdk, wallet, scheme, data, size, signature, capacity)` | Output capacity >=96 for BLS or >=65 for secp. Only that prefix written; input/output may overlap. Valid output prefix zeroed on failure, except unknown scheme/insufficient capacity leave it untouched. |
| `fiaisdk_wallet_destroy(wallet)` | NULL-safe; wipes/frees keys. No live SDK required. |

All operations except destroy return status codes documented in the public header. Metadata
has no provider operation and returns OK or INVALID_ARGUMENT. Create/import/sign may also
return MODULE_NOT_FOUND, UNAVAILABLE, OUT_OF_MEMORY or INTERNAL_ERROR. BUFFER_TOO_SMALL is
used for signing capacity errors. All pointers are borrowed for calls except returned owning
wallet handles. No C++ object, allocation or exception crosses the C ABI.

The 160-byte V1 metadata layout is pinned by compile-time checks and C99 tests. Binary arrays
have fixed lengths; reserved bytes are zero. They do not contain secrets or NUL-terminated
strings. Public structures are append-only and unknown future tails are never overwritten.

## Lifetime and concurrency

Wallet handles are independent from the SDK that created/imported them. Public metadata can
be read and wallets destroyed after SDK destruction. Signing always takes an explicit live
SDK with a compatible crypto provider, including a different instance if desired. Metadata and
signing may run concurrently; neither SDK nor wallet destruction may race calls using that
object. Caller memory must not be concurrently modified during a call.

The C++17 `fiaisdk::Wallet` in `<fiaisdk/cpp/wallet.hpp>` is move-only and destroys its handle
automatically. Its signing methods borrow the SDK supplied at construction/import, which must
remain alive for signing. A moved-from wallet rejects operations with fiaisdk::Error.
`info()` returns public binary metadata; `address()` and `blsAddress()` return lowercase
40-character hex strings without 0x. `signBls()` and `signDigest()` expose distinct contracts.

```cpp
const fiaisdk::Sdk sdk;
const fiaisdk::Crypto crypto(sdk);
const fiaisdk::Wallet wallet(sdk);
const auto info = wallet.info();
const auto digest = crypto.keccak256("public demo message");
const auto signature = wallet.signDigest(digest);
const bool valid = crypto.verify(FIAISDK_CRYPTO_SCHEME_SECP256K1,
    info.secp256k1_public_key, 65, digest.data(), digest.size(),
    signature.data(), signature.size());
```

## Verification evidence

Tests use public/throwaway keys only. They cover BLS scalar-one compressed G1, the secp256k1
scalar-one EVM address, Keccak empty/abc vectors and BLS signature fixture
`sign_case_7055381f640f2c1d` from [ethereum/bls12-381-tests v0.1.2](https://github.com/ethereum/bls12-381-tests/releases/tag/v0.1.2)
(CC0). No expected value is obtained from Archive binaries. Further tests cover deterministic
signing, wrong messages/recovery IDs, malformed/infinite BLS points, invalid scalar ranges,
provider failures, move ownership and metadata lifetime. Server/chain interoperability is not
claimed by these offline tests.
