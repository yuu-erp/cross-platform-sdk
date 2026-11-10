# ADR-0007: Dual-key offline wallet, BLS12-381 and secp256k1

- Status: Accepted for the first offline wallet slice
- Date: 2026-11-10

## Context

The owner requested checking Archive to identify the existing key/address contract, while
forbidding copied implementation. Observed formats: BLS scalar big-endian 32 bytes, G1
compressed public key 48 bytes, G2 compressed signature 96 bytes with
`BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_POP_`; BLS address is the final 20 bytes of
Keccak-256(compressed public key). The primary address is the final 20 bytes of
Keccak-256(uncompressed secp256k1 public key excluding its 0x04 prefix). ECDSA signs a
32-byte digest and serializes r||s||recovery-id (65 bytes).

These are format observations from Core.Utilities getKeyPair/Sign and finsdk WalletController
PreviewAddressFromPrivateKey, not approval of legacy key generation, parsing, storage or logs.
No Archive implementation is copied. No legacy mnemonic/seed derivation is implemented.

## Decision

Add independent BLS and secp256k1 keys to an in-memory, opaque wallet. New wallets generate
independent secrets: blst KeyGen from CSPRNG IKM for BLS; uniformly sampled valid scalar for
secp256k1. Import explicitly accepts both 32-byte big-endian scalars (callers can supply the
same scalar in both slots to import a compatible legacy pair). Reject zero/out-of-range keys;
never clamp, truncate or silently interpret text as key bytes. Do not export secrets.

Use reviewed upstream libraries, statically linked and pinned by commit:
- blst v0.3.16: e7f90de551e8df682f3cc99067d204d8b90d27ad (Apache-2.0), upstream assembly with portable CPU dispatch (Windows integration currently x64 only).
- bitcoin-core libsecp256k1 v0.7.0: a660a4976efe880bae7982ee410b9e0dc59ac983 (MIT), recovery module enabled.
- OpenSSL 3.2..<4 for SHA-256, CSPRNG and named KECCAK-256. This raises ADR-0006's
  minimum from 3.0 to 3.2; SHA3-256 is never substituted for Keccak-256.

Dependencies are fetched only from pinned upstream commits (or provided via CMake FetchContent
source overrides). Third-party targets do not inherit SDK -Werror policy; code is not modified.
Crypto performs key validation, derivation, signing and signature verification behind its port.
The secp context uses caller-preallocated aligned storage so allocation failure is handled by
SDK exception/status mapping, rather than the library allocating constructor's abort-on-OOM callback.
Wallet application depends only on crypto/api. Only composition root wires concrete providers.

BLS signs raw message bytes with the stated DST, performs subgroup/infinity checks on verify,
and does not implement aggregate signatures or proof-of-possession verification in this slice.
ECDSA signs exactly 32 digest bytes deterministically using libsecp256k1 defaults, low-S,
with recovery ID 0..3. Verify binds recovery ID to the supplied public key; no implicit message
hash or Ethereum personal-message prefix. Chain transaction framing is a future module concern.

Wallet handles own their keys and public metadata. Destroy wipes secrets and does not need a
live SDK. Signing explicitly receives a live SDK; SDK destruction must not race its calls.
Reading metadata/signing is thread-safe; wallet destruction must not race any use of that
wallet. No disk storage, OS vault, mnemonic, backup, transaction or network compatibility
beyond these primitive formats is claimed.

## ABI / compatibility impact

Additive wallet and crypto C functions, header-only wrapper and known-size public metadata
struct with struct_size. Existing status values/layouts are unchanged. Invalid verification
material of the correct length returns success with valid=0; invalid lengths/arguments return
INVALID_ARGUMENT. Secret/key/signature material is never logged.

## References

- [blst](https://github.com/supranational/blst/tree/e7f90de551e8df682f3cc99067d204d8b90d27ad)
- [libsecp256k1](https://github.com/bitcoin-core/secp256k1/tree/a660a4976efe880bae7982ee410b9e0dc59ac983)
- [OpenSSL Keccak](https://docs.openssl.org/3.2/man7/EVP_MD-KECCAK/)
