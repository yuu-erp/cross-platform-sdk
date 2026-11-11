#include "loopback_listener.h"

#include <stdio.h>

loopback_listener test_open_loopback(int listening) {
    loopback_listener listener = {TEST_BAD_SOCKET, {0}};
#if defined(_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return listener;
    }
#endif
    listener.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    test_socklen size = sizeof(address);
    if (listener.socket == TEST_BAD_SOCKET ||
        bind(listener.socket, (struct sockaddr *)&address, size) != 0 ||
        getsockname(listener.socket, (struct sockaddr *)&address, &size) != 0 ||
        (listening && listen(listener.socket, 16) != 0)) {
        if (listener.socket != TEST_BAD_SOCKET) {
            test_close_socket(listener.socket);
        }
#if defined(_WIN32)
        WSACleanup();
#endif
        listener.socket = TEST_BAD_SOCKET;
        return listener;
    }
    (void)snprintf(listener.url, sizeof(listener.url), "tcp://127.0.0.1:%u",
                   (unsigned)ntohs(address.sin_port));
    return listener;
}

void test_close_loopback(loopback_listener listener) {
    if (listener.socket != TEST_BAD_SOCKET) {
        test_close_socket(listener.socket);
#if defined(_WIN32)
        WSACleanup();
#endif
    }
}

int test_socket_readable(test_socket socket, long seconds) {
    fd_set readers;
    struct timeval timeout = {seconds, 0};
    FD_ZERO(&readers);
    FD_SET(socket, &readers);
#if defined(_WIN32)
    return select(0, &readers, NULL, NULL, &timeout);
#else
    return select(socket + 1, &readers, NULL, NULL, &timeout);
#endif
}
