# ADR-0006: OpenSSL 3 provider for chain-independent minimal crypto

- Status: Accepted initial slice; expanded by ADR-0007 for offline wallet primitives
- Date: 2026-11-10

## Context

The first crypto slice needs reviewed primitives behind a testable application port.
Wallet signature schemes and address derivation have not been selected. No Archive code,
wallet derivation, mnemonic format, BLS suite or EVM address format is adopted by this ADR.

## Options considered

1. OpenSSL 3: installed cross-platform dependency with EVP hashing and a private CSPRNG.
2. libsodium: suitable general cryptography, but adds a different provider ecosystem.
3. Curve-specific libraries: evaluate for signing once the wallet scheme is specified.

## Decision

Initial decision: use OpenSSL >= 3.0 and < 4 for SHA-256 and `RAND_priv_bytes_ex`, linked privately and
statically into the SDK. CMake requires an installed, maintained OpenSSL 3 development
package built with position-independent code and the consumer's architecture/runtime.
There is no unpinned dependency download. ADR-0007 raises the minimum to 3.2 for Keccak-256 and adds pinned signing libraries. Deployment must include OpenSSL's Apache-2.0
license/notice obligations. The build records the actual installed version; release builds
must lock and maintain their toolchain/package versions independently.

Each provider owns a private library context, default provider and fetched SHA-256 method.
Each provider call releases its private-context thread resources with `OPENSSL_thread_stop_ex`,
so host threads can outlive SDK destruction after their calls finish. It does not change the host's default context or call global OpenSSL cleanup. Failures return
sanitized SDK errors; no error-queue text, inputs, random output or keys are logged.

The module has application/use cases, infrastructure and API layers. There is no domain layer
until it has domain rules. Both use cases use an application-owned `ICryptoProvider` port;
unit tests inject failures, without reimplementing crypto primitives.

## Consequences

OpenSSL must be provisioned on all build hosts. Only the infrastructure target includes its
headers. Installed consumers link only FiaiSDK and do not need OpenSSL headers or CMake targets.
Static linking avoids adding a separately deployed crypto shared library. The public ABI and
export check continue to expose only `fiaisdk_*`.

A call generates at most 65536 private random bytes. Zero-length generation is rejected.
No entropy fallback is permitted. SHA-256 accepts arbitrary binary input, including empty input.
Signature/key/address APIs are deferred until their contract is selected; SHA-256 is not
Keccak-256 and must not silently stand in for a chain's address hash.

## ABI / compatibility impact

Additive crypto C functions and a header-only C++ wrapper. Existing layouts/statuses remain
unchanged. No wallet API, persisted secret store or chain compatibility is claimed here.

## References

- [OpenSSL EVP digest](https://docs.openssl.org/3.0/man3/EVP_DigestInit/)
- [OpenSSL private random bytes](https://docs.openssl.org/3.0/man3/RAND_bytes/)
- [CMake FindOpenSSL](https://cmake.org/cmake/help/v3.25/module/FindOpenSSL.html)

- [OpenSSL thread cleanup](https://docs.openssl.org/3.0/man3/OPENSSL_init_crypto/)
