# Crypto API

This slice implements SHA-256, Keccak-256, private random bytes and signature verification.
Internal key-generation/derivation/signing primitives support the [offline wallet](wallet.md),
which owns secrets and exposes signing through an opaque handle. Primitive providers and
formats are specified in [ADR-0006](../adr/0006-minimal-crypto-provider.md) and
[ADR-0007](../adr/0007-offline-wallet-key-schemes.md).

## C99

Include `<fiaisdk/crypto.h>` (also included in `<fiaisdk/fiaisdk.h>`).

- `fiaisdk_crypto_sha256(sdk, input, input_size, output, output_size)`: binary input, including
  embedded NUL bytes; NULL input accepted only for empty input. At least 32 writable output
  bytes; only those first 32 are used. Input/output can overlap. Too-small buffers are left
  untouched. On other failures, a non-NULL buffer with capacity >=32 has its first 32 bytes
  zeroed. Invalid input/pointers are INVALID_ARGUMENT; capacity <32 is BUFFER_TOO_SMALL.
- `fiaisdk_crypto_random_bytes(sdk, output, output_size)`: required caller buffer with size
  1..65536. On failure a buffer of valid size is zeroed; invalid sizes are rejected without
  touching memory. Entropy/provider failure returns UNAVAILABLE, without fallback.

All APIs borrow the SDK and buffers for the call. All input/output memory is owned by the
caller. Operations are synchronous and thread-safe; SDK destruction must not race them.
Hash time depends on input length; random generation can block while acquiring entropy.
They can return MODULE_NOT_FOUND, OUT_OF_MEMORY or INTERNAL_ERROR as documented in the header.
No secret or digest is automatically logged. Protect/wipe caller random buffers when used
for keys or seeds. The SDK keeps no copy after the random call returns.

- `fiaisdk_crypto_keccak256`: same pointer/capacity/clearing contract as sha256, but the
  Keccak-256 digest. It is distinct from SHA-256 and NIST SHA3-256.
- `fiaisdk_crypto_verify`: scheme int32, borrowed public key/message/signature, required
  uint8 out_valid (must not alias input), reset to zero on all errors. BLS uses 48-byte G1
  key/96-byte G2 signature and raw bytes with POP signature DST; secp uses 65-byte
  uncompressed key, 32-byte digest and 65-byte low-S r||s||recovery-id. Wrong scheme/lengths
  return INVALID_ARGUMENT; malformed/wrong correct-length material returns OK with valid=0.
  BLS subgroup/infinity and secp recovery-ID binding checks are mandatory, never disabled.

## C++17

`<fiaisdk/cpp/crypto.hpp>` provides a non-owning `fiaisdk::Crypto` view of `Sdk`.

```cpp
const fiaisdk::Sdk sdk;
const fiaisdk::Crypto crypto(sdk);
const auto digest = crypto.sha256(std::string_view("abc"));
// digest is std::array<uint8_t, 32>.
```

`sha256(pointer, size)` handles binary buffers; `sha256(string_view)` hashes exactly the view's
bytes. `randomBytes(pointer, size)` writes caller-owned storage instead of placing secrets in
copyable strings. `keccak256()` accepts the same input forms; `verify()` wraps the signature API and returns bool. Methods throw `fiaisdk::Error` for C status failures. A view must not outlive
its native SDK handle. Examples in `examples/c/main.c` and `examples/cpp/main.cpp` demonstrate
hashing a public demo message without printing cryptographic material.

## Build requirements

Install a maintained **OpenSSL >=3.2 and <4** development package containing static libcrypto (PIC on
Unix); the OpenSSL dependency links `OpenSSL::Crypto`, not its TLS library. CMake's `OPENSSL_USE_STATIC_LIBS` is enabled by this
module. No OpenSSL types/headers are exposed through FiaiSDK's public headers or package.
OpenSSL <3.2/4.x are intentionally rejected for this first provider contract.

- macOS: `brew install openssl@3`; configure with
  `cmake --preset debug -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"`.
- Debian/Ubuntu: use a maintained OpenSSL >=3.2 static development package, or vcpkg
  `openssl:x64-linux` with its toolchain. Ubuntu 24.04's default OpenSSL 3.0 is insufficient
  for the named Keccak provider. CI provisions OpenSSL through vcpkg.
- Windows: vcpkg `openssl:x64-windows-static-md` (matching the SDK's x64 dynamic CRT); use
  the vcpkg toolchain and `-DVCPKG_TARGET_TRIPLET=x64-windows-static-md` during configure,
  so Debug/Release map to matching CRT libraries. A root-directory hint alone is insufficient.
  For example: `cmake --preset msvc -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md`.
- Custom installations: set `OPENSSL_ROOT_DIR`; do not disable version or static-link checks.

If switching between OpenSSL installations in an existing build tree, clear cached
`OPENSSL_INCLUDE_DIR`, `OPENSSL_CRYPTO_LIBRARY`, `OPENSSL_SSL_LIBRARY` entries using CMake `-U`
or use a fresh configure. The standard preset otherwise reuses its cached dependency paths.
libsecp256k1 and blst sources are fetched at immutable upstream commits by CMake (ADR-0007).
An initial configure needs network access, or explicit `FETCHCONTENT_SOURCE_DIR_SECP256K1`
and `FETCHCONTENT_SOURCE_DIR_BLST` source overrides. Their licenses are installed alongside
OpenSSL's. Windows integration currently targets x64; ARM64 requires armasm64 build integration.
Installed SDK consumers only need the SDK package. SDK distribution must preserve the OpenSSL
[license notices](../third-party/OpenSSL-LICENSE.txt) (installed with the SDK), and release tooling must track/maintain the actual dependency version.

## Validation

Tests cover published SHA-256 empty/`abc`/million-`a` vectors, binary data and overlapping C
buffers, invalid arguments/capacity, provider failures, module lifecycle, concurrent calls,
C99/header-only C++17 use and the exported symbol list. Random tests check API behavior and
sizes, not probabilistic assertions or claims that statistical tests prove security.
