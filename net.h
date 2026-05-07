/*
net.h Declares the UDP socket and address helpers used by the MustangChat client
and server.

Because the assignment asks to uses UDP instead of TCP, I keep socket creation +
binding + address comparison separate from packet formatting.
*/

#ifndef CHAT_NET_H
#define CHAT_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>

enum
{
    CHAT_ADDR_STRING_SIZE = 128
};

int chat_create_client_socket(const char *host, const char *port);
int chat_create_server_socket(const char *host, const char *port);
void chat_addr_to_string(const struct sockaddr_storage *addr, socklen_t addr_len, char *out, size_t out_size);
bool chat_addr_equal(const struct sockaddr_storage *a, socklen_t a_len, const struct sockaddr_storage *b, socklen_t b_len);

#endif
