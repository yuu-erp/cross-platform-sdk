---
paths:
  - "src/modules/wallet/**"
  - "src/modules/auth/**"
  - "src/modules/crypto/**"
  - "src/modules/smc/**"
  - "src/modules/storage/**"
---
# Security-sensitive modules

- Never implement crypto primitives (hash, sign, KDF, RNG, ciphers). Use the library chosen in
  an ADR (e.g. libsodium / secp256k1 / OpenSSL) behind a port.
- Secrets (private keys, seeds, mnemonics, tokens, passwords): store in a `SecureBuffer` that
  zeroes on destruction; never copy into `std::string`; never log, print, or put in error messages.
- Persist secrets only through the OS keystore port (DPAPI / Keychain / libsecret).
- Validate all external input (addresses, hex, JSON, chain ids, lengths) at the application
  boundary before domain objects are built.
- TLS and signature verification are never disabled, not even behind a debug flag.
- Constant-time comparison for MACs/signatures. Randomness only from the crypto port.
- Tests use generated throwaway keys or published test vectors — never real keys.
- Any change here: mention security impact in the PR description.
