# Network API

The default composition opens a TCP socket to one endpoint per SDK instance. It accepts
`tcp://127.0.0.1:9545` and `tcp://[::1]:9545` forms. Use a numeric IPv4 or IPv6 address and an
explicit port from 1 to 65535. The TCP adapter does not resolve DNS names. Credentials, paths, queries and fragments are invalid.
The HTTP and WebSocket URLs used by the previous simulated adapter are unsupported by the
default TCP adapter.

`CONNECTED` means the SDK holds a socket that completed the TCP handshake.
This transport uses plaintext TCP. Meta-Node initialization is pending. `latency_ms` measures
the time spent establishing the socket. Status and endpoint reads check for an observable peer
close and clear the session when detected. The check preserves incoming data. A close behind
unread data can remain undetected, and a silent network outage may produce no socket error.

- `fiaisdk_network_connect` accepts 1–60000 ms, including time spent waiting for a concurrent
  operation. Invalid or unsupported endpoints return `INVALID_ARGUMENT`. Socket errors
  such as connection refusal or timeout return `UNAVAILABLE`.
- Connecting again to the same endpoint returns OK. A successful switch closes the old
  socket. A failed switch preserves the old connection.
- `fiaisdk_network_disconnect` waits for any connect in progress and closes the socket.
  Calling it while disconnected succeeds. SDK destruction also closes the socket.
- Status reads do not wait for network I/O. Initialize the extensible 24-byte status struct
  with `FIAISDK_NETWORK_STATUS_INIT`. Unknown future fields are preserved.
- Endpoint reads return the normalized URL, or an empty string while disconnected.
  Query the required size with a null buffer and size zero. The size includes the NUL
  terminator. An undersized buffer is left untouched.
- SDK destruction must not race operations using the SDK instance.

## C and C++

Use an already listening endpoint for these examples.

```c
#include <fiaisdk/fiaisdk.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const fiaisdk_sdk_config config = FIAISDK_SDK_CONFIG_INIT;
    fiaisdk_sdk *sdk = NULL;
    if (fiaisdk_sdk_create(&config, &sdk) != FIAISDK_STATUS_OK) {
        return 1;
    }

    fiaisdk_status result = fiaisdk_network_connect(sdk, "tcp://127.0.0.1:9545", 5000);
    if (result == FIAISDK_STATUS_OK) {
        fiaisdk_network_status status = FIAISDK_NETWORK_STATUS_INIT;
        result = fiaisdk_network_get_status(sdk, &status);
        if (result == FIAISDK_STATUS_OK && status.state == FIAISDK_NETWORK_STATE_CONNECTED) {
            size_t size = 0;
            if (fiaisdk_network_get_endpoint(sdk, NULL, &size) == FIAISDK_STATUS_BUFFER_TOO_SMALL) {
                char *endpoint = malloc(size);
                if (endpoint != NULL) {
                    if (fiaisdk_network_get_endpoint(sdk, endpoint, &size) == FIAISDK_STATUS_OK) {
                        puts(endpoint);
                    }
                    free(endpoint);
                }
            }
        }
        fiaisdk_network_disconnect(sdk);
    }
    fiaisdk_sdk_destroy(sdk);
    return result == FIAISDK_STATUS_OK ? 0 : 1;
}
```

```cpp
fiaisdk::Sdk sdk;
fiaisdk::Network network(sdk);
network.connect("tcp://127.0.0.1:9545", std::chrono::seconds(5));
const auto status = network.status();
network.disconnect();
```

Run the C or C++ example executable with the TCP URL as its first argument.
Without an endpoint, the examples exercise only the offline SDK operations.

## Internal events

`network.connected` reports the sequence, origin and connection time.
`network.disconnected` reports the sequence, origin and reason (requested, endpoint switch
peer closure or shutdown). Events publish after state commits and outside session locks. Events from
different threads can arrive out of order. Consumers can use the sequence to discard stale
events or read the current status again.
