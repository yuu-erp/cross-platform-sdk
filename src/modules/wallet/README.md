# wallet module

Wallet owns a BLS/secp256k1 key pair and exposes offline creation, import, public metadata,
signing and encrypted persistence. [ADR-0007](../../../docs/adr/0007-offline-wallet-key-schemes.md)
defines the key formats, and [ADR-0010](../../../docs/adr/0010-sqlite-wallet-store.md) defines the store.

The domain owns Wallet and WalletInfo. Application owns CreateWallet, ImportWallet,
SignWithWallet and the IWalletStore port. Infrastructure implements the port with SQLite
and OpenSSL, and bootstrap selects the adapter. The facade reuses ImportWallet to validate
stored keys and rebuild public metadata. The public store is independent of SDK lifetime.

Each new wallet generates independent keys. Import explicitly supplies two big-endian scalar
buffers, validated without clamping. Public metadata includes the secp-based address and BLS
address; address derivation uses Keccak-256, never SHA3-256. Signing delegates to crypto.

Wallet handles own keys independently of SDK lifetime. Metadata and destruction do not need
an SDK; signing borrows an explicit live SDK. Read/sign operations are thread-safe; destruction
must not race users. The C++ owning wrapper also borrows the original SDK for later signing.
