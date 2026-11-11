# ADR-0010 — Encrypted SQLite wallet storage

Status: Accepted

The offline wallet needs a file-backed store for saving, reopening, listing and deleting
wallets. Archive's wallet repository informed those operations. The new database has its own
schema and stores only data needed to recover and use an offline wallet.

- Statically link the official SQLite 3.54.1 amalgamation, pinned by SHA-256 in CMake. SQLite
  is public domain. Disable extension loading and URI filename interpretation in the SDK build.
- Keep SQL in wallet infrastructure. Key wrapping uses the crypto module's AES-256-GCM
  (`CryptoFacade::aeadSeal/aeadOpen`, ADR-0006); wallet infrastructure does not link OpenSSL.
  Bootstrap gives each opened store its own crypto provider, created once per store, so the
  store stays independent of SDK lifetime. The application port returns decrypted
  scalars in a zeroing buffer. The wallet facade uses the existing import path to validate
  both scalars and derive metadata. Bootstrap selects the adapter.
- The caller chooses a dedicated database file and supplies a dedicated random 256-bit key.
  AES-256-GCM encrypts the two scalars with a fresh random 96-bit nonce and a 128-bit
  tag per record. The primary 20-byte wallet address is public and used as authenticated
  associated data. SQLite receives only the address, nonce, ciphertext and tag.
- Schema v1 uses a unique primary address and an authenticated key verifier in a separate
  table. The verifier encrypts 64 zero bytes with empty associated data. An immediate
  transaction initializes the schema and verifier together. Reject incompatible versions and
  existing nonempty unversioned databases. Duplicate saves leave the original row intact.
- A wrong store key returns `FIAISDK_STATUS_AUTHENTICATION_FAILED` (9), distinct from invalid
  arguments, so hosts can report it. `NOT_FOUND` (7) and `ALREADY_EXISTS` (8) are the other new
  status codes; codes 101/102 from the first draft were never released.
- `PRAGMA secure_delete=ON` overwrites removed records instead of leaving them in free pages.
- The key and file are managed by the host application. Existing in-memory wallet functions
  are still available. The added C functions use opaque handles and the C++ wrapper remains
  header-only. There is no Archive schema migration or password/key derivation in this release.

[SQLite amalgamation](https://www.sqlite.org/amalgamation.html) ·
[SQLite license](https://www.sqlite.org/copyright.html) ·
[OpenSSL EVP](https://docs.openssl.org/3.6/man3/EVP_EncryptInit/)
