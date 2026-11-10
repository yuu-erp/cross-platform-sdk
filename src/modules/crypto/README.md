# crypto module

Responsibility: cryptographic primitives through reviewed, pinned library providers. No
Archive implementation is copied. Contracts/dependencies: ADR-0006 and ADR-0007.

Implemented synchronously: SHA-256, Keccak-256, private random bytes, BLS12-381 and secp256k1
key generation/derivation/sign/verify. Application use cases expose execute(); provider port is
application-owned. The domain layer contains only key-scheme identity. LibraryCryptoProvider
uses OpenSSL 3.2+, blst and libsecp256k1; concrete wiring is only in composition_root.cpp.

BLS uses compressed G1 keys (48 bytes), G2 signatures (96), the POP signature DST in ADR-0007
and subgroup/infinity checks. secp256k1 signs 32 digest bytes with deterministic low-S
recoverable signatures (r||s||recovery-id 0..3). No aggregate, encryption or transaction codec.

The provider owns a private OpenSSL context, cleans per-thread resources after each call and
never changes host defaults. Its secp context is randomized once before publication, then
read-only. Secret scalars/IKM use wiping buffers; errors never include key material.
Methods are thread-safe; destruction must not race calls. Stateless facade construction is
available before module initialization so composition root can wire dependent modules; no
consumer is exposed through the C ABI until SDK initialization completes.
