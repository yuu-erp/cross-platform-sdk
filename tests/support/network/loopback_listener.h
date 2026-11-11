#ifndef FIAISDK_TEST_LOOPBACK_LISTENER_H
#define FIAISDK_TEST_LOOPBACK_LISTENER_H

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket;
typedef int test_socklen;
#define TEST_BAD_SOCKET INVALID_SOCKET
#define test_close_socket closesocket
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int test_socket;
typedef socklen_t test_socklen;
#define TEST_BAD_SOCKET (-1)
#define test_close_socket close
#endif

typedef struct loopback_listener {
    test_socket socket;
    char url[32];
} loopback_listener;

#ifdef __cplusplus
extern "C" {
#endif

loopback_listener test_open_loopback(int listening);
void test_close_loopback(loopback_listener listener);
int test_socket_readable(test_socket socket, long seconds);

#ifdef __cplusplus
}
#endif

#endif
