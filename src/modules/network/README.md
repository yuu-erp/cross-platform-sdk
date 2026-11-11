# network module

The default adapter opens a TCP connection to a numeric IPv4 or IPv6 address and keeps the
socket until disconnect, replacement or SDK shutdown. Meta-Node initialization and an
alternate RPC adapter are future steps. The TCP adapter requires a numeric host and performs no
DNS resolution. Read status through the typed `fiaisdk_network_get_status` C API.

- Domain parses and normalizes endpoint URLs.
- Application owns the connector port, session state, operation deadline and lifecycle events.
- Infrastructure implements the TCP connector. Native socket code lives under `src/platform`.
- The API facade exposes the use cases. The composition root selects the concrete connector.

A connector returns an owning connection object plus connection time. The application stores
it with the metadata only after opening succeeds. An alternate RPC implementation can own a
logical session through the same port. Status reads use a separate lock from the operation
gate. Tests inject fake connectors or use local TCP listeners.

Public contracts and examples are in [the network API guide](../../../docs/api/network.md).
