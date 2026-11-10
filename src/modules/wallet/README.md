# wallet module

Responsibility: own an in-memory BLS/secp256k1 key pair and expose offline creation, import,
public metadata and signing without exporting secrets. ADR-0007 defines formats and scope.
No Archive implementation, mnemonic scheme, database or encryption workflow is copied.

Layers: domain Wallet/WalletInfo; application CreateWallet/ImportWallet/SignWithWallet with
execute(); API facade/module. No infrastructure layer is needed yet: crypto/api provides the
primitive boundary and keys live only in SecureBytes. No repository, global registry or disk
persistence is introduced. Depends on crypto (facade only) and core domain/application.

Each new wallet generates independent keys. Import explicitly supplies two big-endian scalar
buffers, validated without clamping. Public metadata includes the secp-based address and BLS
address; address derivation uses Keccak-256, never SHA3-256. Signing delegates to crypto.

Wallet handles own keys independently of SDK lifetime. Metadata and destruction do not need
an SDK; signing borrows an explicit live SDK. Read/sign operations are thread-safe; destruction
must not race users. The C++ owning wrapper also borrows the original SDK for later signing.
