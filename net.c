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

