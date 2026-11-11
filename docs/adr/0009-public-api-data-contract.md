# ADR-0009: Public API data contract — typed C ABI, no JSON in the core

- Status: Accepted
- Date: 2026-11-11
- Deciders: project owner
- Related: ADR-0002 (hourglass C ABI), ADR-0003 (naming), ADR-0007 (wallet formats)

## Context
FiaiSDK replaces the Archive system (`windowrunsdk`, `windowfinsdk`, `windowcore`), not just its
native layer. The new SDK therefore defines the contract every host builds against; there is no
legacy consumer whose request format must be preserved.

Archive exposes one dynamic entry point, `RunSDKWrapper.Call(JsonObject)`. Each request is
`{"command": "<name>", ...}`; the wrapper looks the command up in about twenty string lists
(`MainControllerMethod`, `TransactionControllerMethod`, ...) and returns
`{success, code, data}`. Events are pushed through one `Action<JsonObject>` callback.
Observed consequences:
- Command names, field names and field types are checked only at run time; naming is mixed
  (`createSign`, `get-backup-files`, `read-from-local-storage...`).
- No contract exists outside controller code; changing a field silently breaks callers.
- Secrets and signing material travel in JSON strings (`createSign`, `decryptAesECDH`), where
  they are copied, logged and cannot be wiped.
- Error codes are ad hoc strings per call site (`ENSURE_NODE_NO_LAST`, ...).

The uncommitted network work added `fiaisdk_network_*_json` functions returning
`{success, code, message, data}` strings, re-introducing the same pattern into the stable ABI.

## Options considered
1. **JSON envelope as the public ABI** (Archive model, one `call(json) -> json` or per-function
   JSON). Pros: one entry point, easy for JS/webview. Cons: all problems above; every JSON field
   becomes an unversioned ABI promise; parse cost; secrets in strings; no compiler or ABI-test help.
2. **Binary schema as the public ABI** (protobuf/FlatBuffers/CBOR). Pros: schema, versioning,
   codegen. Cons: adds a dependency and generated code to every consumer of the C ABI, still a
   dynamic dispatch surface, still serialises secrets.
3. **Typed C ABI as the contract; message protocols only as optional adapters on top.**
   Pros: compile-time checking, ABI tests, explicit ownership/wiping of secrets, matches
   ADR-0002 and existing rules. Cons: more exported functions; a message-based host needs an
   adapter.

## Decision
Option 3.

### Layers
| Layer | Request | Response |
|---|---|---|
| domain / application | strong C++ types (`Endpoint`, `KeyScheme`, `std::span<const std::uint8_t>`) | `core::Result<T>` with `core::Error` |
| public C ABI (`include/fiaisdk/*.h`) | typed parameters, extensible structs, opaque handles | `fiaisdk_status` return + typed `out_` parameters |
| C++ wrapper (`include/fiaisdk/cpp/`) | C++ types mapped 1:1 to the C ABI | C++ `Result`-style values over the same statuses |
| message adapters (optional, future) | schema-defined, versioned envelope | schema-defined envelope with `id`, `result` or `error{code}` |

### Public C ABI data conventions
- **Binary data** (keys, hashes, signatures, addresses, payloads): `const uint8_t *` + `size_t`
  in; caller buffer + capacity out, or fixed-size `uint8_t[N]` fields in a struct. Never hex,
  base64 or decimal strings.
- **Text**: UTF-8, NUL-terminated `const char *` in. Out via caller buffer + `size_t *inout_size`
  (`BUFFER_TOO_SMALL` reports the required size) or an SDK-owned object with a matching free.
- **Fixed-width integers only.** Booleans are `uint8_t` (0/1). Enumerations are `int32_t`
  constants; values are never renumbered.
- **256-bit amounts and chain integers**: 32-byte big-endian unsigned bytes. No floating point
  and no decimal strings for value-bearing quantities.
- **Time**: instants are `int64_t` Unix milliseconds (UTC); durations are `uint32_t` milliseconds.
- **Records**: extensible structs (`uint32_t struct_size` first, append-only, `_V1_SIZE` and
  `_INIT` macros), as in `fiaisdk_network_status` and `fiaisdk_wallet_info`.
- **Lists**: caller buffer + element count with the two-call size protocol, or an SDK-owned
  opaque result handle with accessor and destroy functions. Choose per API; document it.
- **Errors**: the single `fiaisdk_status` table is the machine-readable result for every module.
  When an operation needs more detail, it adds a typed out-struct or an operation-specific
  status constant — never a free-text or JSON error payload. `fiaisdk_status_message` is for
  humans only and is not a stable contract.
- **Presentation encodings** (hex, EIP-55 checksum addresses, base64, decimal amounts with
  token decimals) are not canonical representations. If several hosts need the same formatting,
  provide explicit pure formatting/parsing functions; canonical inputs and outputs stay bytes.
- **Secrets** enter only as byte buffers or through opaque handles (`fiaisdk_wallet`) and are
  never returned inside a string, JSON document or log message.

### Asynchrony and events
- Asynchronous operations use `callback + void *user_data + request id`, with the callback thread,
  cancellation and timeout documented per function. The executor, cancellation function and
  thread guarantees are decided by a dedicated ADR before the first asynchronous public function.
- Pushed events use one typed callback type per event family, registered and unregistered
  through a subscription handle. There is no catch-all JSON event callback.

### Message adapters
A host that needs message passing (dApp/webview bridge, IPC, remote control) gets an adapter
that is:
- separate from the core shared library, implemented only against the public C ABI or the
  C++ wrapper, so the core ABI never depends on it;
- schema-first and versioned; the format (e.g. JSON-RPC 2.0 or protobuf) is chosen by a
  separate ADR when the first real host needs it;
- an allow-list: each message maps to exactly one typed function, and operations that take raw
  secret material are not exposed to untrusted callers such as dApps.

## Consequences
- The `fiaisdk_network_*_json` functions are removed before they are committed; the Qt example
  uses the typed network API.
- Each new capability adds typed functions, ABI tests and documentation; the export list grows.
- Adding a message bridge later costs one adapter, without changing core business logic.
- Reviewers reject public functions that accept or return JSON, hex or other encoded text as the
  canonical representation of structured or binary data.

## ABI / compatibility impact
None for released interfaces: the JSON functions were never part of a commit or release.
Existing functions already follow these conventions. Future public APIs must follow them.
