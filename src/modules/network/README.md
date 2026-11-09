# network module

Status: **demo vertical slice** — real layering and C ABI, simulated transport.

- Responsibility: manage the SDK's connection to one Meta-Node node endpoint (connect,
  disconnect, status) and announce changes to other modules through events.
- Depends on: core only. Other modules depend on it (via `api/` and its events).
- Thread-safety: all facade operations are thread-safe; connect/disconnect are serialised.

| Layer | Files | Purpose |
|---|---|---|
| domain | `endpoint.*` | `Endpoint` value object: URL parsing, TLS policy, no credentials |
| | `connection_info.h` | current connection facts |
| | `network_events.h` | `NetworkConnected`, `NetworkDisconnected` (origin only, no path) |
| application | `ports/i_node_probe.h` | port: "does this node answer within timeout?" |
| | `network_session.h` | thread-safe current-connection state + change sequence numbers |
| | `connect_to_node.*`, `disconnect_from_node.*` | use cases |
| infrastructure | `simulated_node_probe.*` | **DEMO** probe, no I/O (`*.invalid` = unreachable) |
| api | `network_facade.*`, `network_module.*` | entry point for bindings + lifecycle |

Public C API: `include/fiaisdk/network.h`; C++: `include/fiaisdk/cpp/network.hpp`.

Replacing the demo transport: implement `INodeProbe` in `infrastructure/` with the HTTP/WebSocket
library chosen by ADR, then swap `SimulatedNodeProbe` in `src/core/composition_root.cpp`.
Nothing in domain/application/api/bindings changes.
