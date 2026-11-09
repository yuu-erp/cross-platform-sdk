# Network API (`fiaisdk/network.h`, `fiaisdk/cpp/network.hpp`)

> **Demo transport.** This version simulates the node (no network I/O): hosts ending in
> `.invalid` are unreachable, every other valid host connects with a deterministic 5–49 ms
> latency. The API is intended to stay stable when the real HTTP/WebSocket transport lands.

## Functions

| Function | Purpose | Blocks | Returns |
|---|---|---|---|
| `fiaisdk_network_connect(sdk, url, timeout_ms)` | connect, replacing the current connection on success | ≤ ~`timeout_ms` in total (incl. waiting for a concurrent call) | `OK`, `INVALID_ARGUMENT`, `UNAVAILABLE`, `MODULE_NOT_FOUND`, `OUT_OF_MEMORY` |
| `fiaisdk_network_disconnect(sdk)` | close the connection (OK if none) | waits for a connect in progress | `OK`, `INVALID_ARGUMENT` |
| `fiaisdk_network_get_status(sdk, &status)` | state, latency, connected-at | never waits for a connect | `OK`, `INVALID_ARGUMENT` |
| `fiaisdk_network_get_endpoint(sdk, buf, &size)` | normalised URL (two-call size protocol) | no | `OK`, `BUFFER_TOO_SMALL`, `INVALID_ARGUMENT` |

All functions are thread-safe; connect/disconnect calls are serialised. None may run concurrently
with `fiaisdk_sdk_destroy`. Every function may also return `MODULE_NOT_FOUND`, `OUT_OF_MEMORY` or
`INTERNAL_ERROR`. `get_endpoint` sets `*size` to the required size on `OK`/`BUFFER_TOO_SMALL`
and to 0 on any other error.

## Behaviour

- Connecting again to the same endpoint (after normalisation) is a no-op returning `OK`.
- Switching endpoints is all-or-nothing: if the new node is unreachable the old connection stays.
- `timeout_ms` must be 1..60000.
- Destroying the SDK while connected disconnects cleanly.

## Endpoint rules

| Rule | Example rejected |
|---|---|
| scheme `https`, `wss`; `http`/`ws` only for exactly `localhost`, `127.0.0.1`, `[::1]` | `http://node.example.com`, `http://app.localhost` |
| no credentials in the URL | `https://user:pass@node.example.com` |
| host = ASCII DNS name / IPv4 / `[IPv6]`; labels 1–63 chars, no leading/trailing `-` | `https://node_x.example.com`, `https://-a.example.com` |
| port 1..65535 (default 443 / 80) | `https://node.example.com:0` |
| ≤ 2048 chars, no whitespace | `https://node.example.com/a b` |

Normalisation: scheme and host lower-cased, default port removed, path/query kept as given.
The path/query may contain API keys — the SDK never logs it and events carry only the origin
(`https://host:port`).

## `fiaisdk_network_status` (24 bytes, all platforms)

| Offset | Field | Meaning |
|---|---|---|
| 0 | `uint32_t struct_size` | set by caller (`FIAISDK_NETWORK_STATUS_INIT`) |
| 4 | `int32_t state` | `FIAISDK_NETWORK_STATE_DISCONNECTED` / `_CONNECTED` |
| 8 | `int64_t connected_at_unix_ms` | 0 when disconnected |
| 16 | `uint32_t latency_ms` | 0 when disconnected |
| 20 | `uint32_t reserved0` | always 0 |

## C

```c
fiaisdk_status s = fiaisdk_network_connect(sdk, "https://node.example.com/rpc", 5000);
if (s == FIAISDK_STATUS_UNAVAILABLE) { /* retry later */ }

fiaisdk_network_status st = FIAISDK_NETWORK_STATUS_INIT;
fiaisdk_network_get_status(sdk, &st);

size_t size = 0;
fiaisdk_network_get_endpoint(sdk, NULL, &size);   /* BUFFER_TOO_SMALL, size = needed */
char *url = malloc(size);
fiaisdk_network_get_endpoint(sdk, url, &size);
```

## C++

```cpp
fiaisdk::Sdk sdk;
fiaisdk::Network network(sdk);          // must not outlive sdk
network.connect("wss://node.example.com/ws", std::chrono::seconds(3));  // throws fiaisdk::Error
auto status = network.status();          // state, latency, connectedAt
std::string url = network.endpoint();
```

## Events (inside the SDK)

| Event | `name()` | Data |
|---|---|---|
| `network::NetworkConnected` | `network.connected` | sequence, origin, latency |
| `network::NetworkDisconnected` | `network.disconnected` | sequence, origin, reason (`Requested`, `SwitchingEndpoint`, `Shutdown`) |

Events are published after the state changed, outside locks, so subscribers on different
threads may see them out of order: `sequence()` grows with every change — ignore an event whose
sequence is lower than the last one handled, or re-read the status.

Other modules subscribe with `core::subscribeTo<network::NetworkConnected>(context.eventBus(), ...)`.
