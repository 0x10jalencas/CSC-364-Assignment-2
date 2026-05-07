/*
 * net.c Implements the UDP socket and address helpers declared in net.h.
 */

#include "net.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Private helpers
 */
static int create_udp_socket(
    const char *host,
    const char *port,
    bool should_bind);

static int create_udp_socket(
    const char *host,
    const char *port,
    bool should_bind)
{
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *current = NULL;
    int result;
    int sockfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (should_bind)
    {
        hints.ai_flags = AI_PASSIVE;
    }

    result = getaddrinfo(host, port, &hints, &results);

    if (result != 0)
    {
        return -1;
    }

    for (current = results; current != NULL; current = current->ai_next)
    {
        sockfd = socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol);

        if (sockfd < 0)
        {
            continue;
        }

        if (should_bind)
        {
            if (bind(sockfd, current->ai_addr, current->ai_addrlen) == 0)
            {
                break;
            }
        }
        else
        {
            if (connect(sockfd, current->ai_addr, current->ai_addrlen) == 0)
            {
                break;
            }
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(results);

    return sockfd;
}

int chat_create_client_socket(
    const char *host,
    const char *port)
{
    return create_udp_socket(host, port, false);
}

int chat_create_server_socket(
    const char *host,
    const char *port)
{
    return create_udp_socket(host, port, true);
}

void chat_addr_to_string(
    const struct sockaddr_storage *addr,
    socklen_t addr_len,
    char *out,
    size_t out_size)
{
    const void *raw_addr = NULL;
    unsigned short port = 0;
    char ip[INET6_ADDRSTRLEN];

    (void)addr_len;

    if (out == NULL || out_size == 0)
    {
        return;
    }

    if (addr == NULL)
    {
        snprintf(out, out_size, "unknown");
        return;
    }

    memset(ip, 0, sizeof(ip));

    if (addr->ss_family == AF_INET)
    {
        const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)addr;

        raw_addr = &ipv4->sin_addr;
        port = ntohs(ipv4->sin_port);
    }
    else if (addr->ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *ipv6 = (const struct sockaddr_in6 *)addr;

        raw_addr = &ipv6->sin6_addr;
        port = ntohs(ipv6->sin6_port);
    }
    else
    {
        snprintf(out, out_size, "unknown");
        return;
    }

    if (inet_ntop(addr->ss_family, raw_addr, ip, sizeof(ip)) == NULL)
    {
        snprintf(out, out_size, "unknown");
        return;
    }

    snprintf(out, out_size, "%s:%u", ip, port);
}

bool chat_addr_equal(
    const struct sockaddr_storage *a,
    socklen_t a_len,
    const struct sockaddr_storage *b,
    socklen_t b_len)
{
    if (a == NULL || b == NULL)
    {
        return false;
    }

    if (a->ss_family != b->ss_family)
    {
        return false;
    }

    if (a->ss_family == AF_INET)
    {
        const struct sockaddr_in *a4 = (const struct sockaddr_in *)a;
        const struct sockaddr_in *b4 = (const struct sockaddr_in *)b;

        return a4->sin_port == b4->sin_port &&
               memcmp(&a4->sin_addr, &b4->sin_addr, sizeof(a4->sin_addr)) == 0;
    }

    if (a->ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *b6 = (const struct sockaddr_in6 *)b;

        return a6->sin6_port == b6->sin6_port &&
               a6->sin6_scope_id == b6->sin6_scope_id &&
               memcmp(&a6->sin6_addr, &b6->sin6_addr, sizeof(a6->sin6_addr)) == 0;
    }

    if (a_len != b_len)
    {
        return false;
    }

    return memcmp(a, b, a_len) == 0;
}
