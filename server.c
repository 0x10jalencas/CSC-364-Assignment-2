/*
server.c Implements the MustangChat server.

Receives UDP requests, updates server-side user/channel state, relays chat
messages, and removes inactive clients using soft-state timeouts.
 */

#include "net.h"
#include "protocol.h"
#include "server_state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum
{
    RECV_PACKET_SIZE = 4096,
    SEND_PACKET_SIZE = 4096,

    SERVER_SWEEP_SECONDS = 120,
    SERVER_TIMEOUT_SECONDS = 120,

    REQ_NAME_OFFSET = CHAT_U32_SIZE,

    REQ_SAY_CHANNEL_OFFSET = CHAT_U32_SIZE,
    REQ_SAY_TEXT_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE
};

/*
 * Private helpers
 */
static void usage(
    const char *program);

static void send_packet(
    int sockfd,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *addr,
    socklen_t addr_len);

static void send_error(
    int sockfd,
    const char *message,
    const struct sockaddr_storage *addr,
    socklen_t addr_len);

static bool packet_has_size(
    size_t packet_size,
    size_t needed_size);

static int find_logged_in_user(
    ChatServerState *state,
    const struct sockaddr_storage *addr,
    socklen_t addr_len,
    time_t now);

static void handle_packet(
    int sockfd,
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_login(
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_join(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_leave(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_say(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_list(
    int sockfd,
    ChatServerState *state,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void handle_who(
    int sockfd,
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len);

static void sweep_timeouts(
    ChatServerState *state,
    time_t now);

static void debug_request(
    const ChatServerState *state,
    int user_index,
    const char *channel,
    const char *message);

int main(
    int argc,
    char **argv)
{
    const char *host;
    const char *port;
    int sockfd;
    ChatServerState state;
    time_t last_sweep;

    if (argc != 3)
    {
        usage(argv[0]);
        return 1;
    }

    host = argv[1];
    port = argv[2];

    sockfd = chat_create_server_socket(host, port);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    chat_server_state_init(&state);
    last_sweep = time(NULL);

    while (true)
    {
        fd_set readfds;
        struct timeval timeout;
        int ready;
        time_t now;

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0)
        {
            perror("select");
            break;
        }

        if (ready > 0 && FD_ISSET(sockfd, &readfds))
        {
            uint8_t packet[RECV_PACKET_SIZE];
            struct sockaddr_storage client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            ssize_t packet_size;

            packet_size = recvfrom(
                sockfd,
                packet,
                sizeof(packet),
                0,
                (struct sockaddr *)&client_addr,
                &client_addr_len);

            if (packet_size > 0)
            {
                handle_packet(
                    sockfd,
                    &state,
                    packet,
                    (size_t)packet_size,
                    &client_addr,
                    client_addr_len);
            }
        }

        now = time(NULL);

        if (now - last_sweep >= SERVER_SWEEP_SECONDS)
        {
            sweep_timeouts(&state, now);
            last_sweep = now;
        }
    }

    close(sockfd);

    return 0;
}

static void usage(
    const char *program)
{
    fprintf(stderr, "Usage: %s <host> <port>\n", program);
}

static void send_packet(
    int sockfd,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *addr,
    socklen_t addr_len)
{
    if (sendto(
            sockfd,
            packet,
            packet_size,
            0,
            (const struct sockaddr *)addr,
            addr_len) < 0)
    {
        perror("sendto");
    }
}

static void send_error(
    int sockfd,
    const char *message,
    const struct sockaddr_storage *addr,
    socklen_t addr_len)
{
    uint8_t packet[CHAT_ERROR_RESP_SIZE];
    size_t packet_size;

    packet_size = chat_build_error_response(packet, message);
    send_packet(sockfd, packet, packet_size, addr, addr_len);
}

static bool packet_has_size(
    size_t packet_size,
    size_t needed_size)
{
    return packet_size >= needed_size;
}

static int find_logged_in_user(
    ChatServerState *state,
    const struct sockaddr_storage *addr,
    socklen_t addr_len,
    time_t now)
{
    int user_index;

    user_index = chat_server_find_user_by_addr(state, addr, addr_len);

    if (user_index >= 0)
    {
        chat_server_touch_user(state, user_index, now);
    }

    return user_index;
}

static void handle_packet(
    int sockfd,
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    uint32_t type;
    int user_index;
    time_t now;

    if (!packet_has_size(packet_size, CHAT_U32_SIZE))
    {
        return;
    }

    type = chat_read_u32(packet);

    if (!chat_is_request_type(type))
    {
        return;
    }

    if (type == CHAT_REQ_LOGIN)
    {
        handle_login(state, packet, packet_size, client_addr, client_addr_len);
        return;
    }

    now = time(NULL);
    user_index = find_logged_in_user(state, client_addr, client_addr_len, now);

    if (user_index < 0)
    {
        return;
    }

    switch (type)
    {
    case CHAT_REQ_LOGOUT:
        debug_request(state, user_index, "", "logout");
        chat_server_logout_user(state, user_index);
        break;

    case CHAT_REQ_JOIN:
        handle_join(sockfd, state, user_index, packet, packet_size, client_addr, client_addr_len);
        break;

    case CHAT_REQ_LEAVE:
        handle_leave(sockfd, state, user_index, packet, packet_size, client_addr, client_addr_len);
        break;

    case CHAT_REQ_SAY:
        handle_say(sockfd, state, user_index, packet, packet_size, client_addr, client_addr_len);
        break;

    case CHAT_REQ_LIST:
        debug_request(state, user_index, "", "list");
        handle_list(sockfd, state, client_addr, client_addr_len);
        break;

    case CHAT_REQ_WHO:
        handle_who(sockfd, state, packet, packet_size, client_addr, client_addr_len);
        break;

    case CHAT_REQ_KEEPALIVE:
        debug_request(state, user_index, "", "keepalive");
        break;

    default:
        break;
    }
}

static void handle_login(
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char user[CHAT_NAME_SIZE + 1];
    int user_index;

    if (!packet_has_size(packet_size, CHAT_LOGIN_REQ_SIZE))
    {
        return;
    }

    chat_read_fixed_string(user, packet + REQ_NAME_OFFSET, CHAT_NAME_SIZE);

    user_index = chat_server_login_user(
        state,
        user,
        client_addr,
        client_addr_len,
        time(NULL));

    if (user_index >= 0)
    {
        debug_request(state, user_index, "", "login");
    }
}

static void handle_join(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char channel[CHAT_NAME_SIZE + 1];

    if (!packet_has_size(packet_size, CHAT_JOIN_REQ_SIZE))
    {
        return;
    }

    chat_read_fixed_string(channel, packet + REQ_NAME_OFFSET, CHAT_NAME_SIZE);
    debug_request(state, user_index, channel, "join");

    if (!chat_server_join_channel(state, user_index, channel))
    {
        send_error(sockfd, "could not join channel", client_addr, client_addr_len);
    }
}

static void handle_leave(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char channel[CHAT_NAME_SIZE + 1];

    if (!packet_has_size(packet_size, CHAT_LEAVE_REQ_SIZE))
    {
        return;
    }

    chat_read_fixed_string(channel, packet + REQ_NAME_OFFSET, CHAT_NAME_SIZE);
    debug_request(state, user_index, channel, "leave");

    if (!chat_server_leave_channel(state, user_index, channel))
    {
        send_error(sockfd, "could not leave channel", client_addr, client_addr_len);
    }
}

static void handle_say(
    int sockfd,
    ChatServerState *state,
    int user_index,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char channel[CHAT_NAME_SIZE + 1];
    char text[CHAT_TEXT_SIZE + 1];
    int members[CHAT_SERVER_MAX_USERS];
    int member_count;
    int i;
    uint8_t response[SEND_PACKET_SIZE];
    size_t response_size;

    if (!packet_has_size(packet_size, CHAT_SAY_REQ_SIZE))
    {
        return;
    }

    chat_read_fixed_string(channel, packet + REQ_SAY_CHANNEL_OFFSET, CHAT_NAME_SIZE);
    chat_read_fixed_string(text, packet + REQ_SAY_TEXT_OFFSET, CHAT_TEXT_SIZE);

    debug_request(state, user_index, channel, text);

    if (!chat_server_user_in_channel(state, user_index, channel))
    {
        send_error(sockfd, "you are not in that channel", client_addr, client_addr_len);
        return;
    }

    member_count = chat_server_get_channel_members(
        state,
        channel,
        members,
        CHAT_SERVER_MAX_USERS);

    response_size = chat_build_say_response(
        response,
        channel,
        state->users[user_index].user,
        text);

    for (i = 0; i < member_count; i++)
    {
        int member_index = members[i];

        if (state->users[member_index].in_use)
        {
            send_packet(
                sockfd,
                response,
                response_size,
                &state->users[member_index].addr,
                state->users[member_index].addr_len);
        }
    }
}

static void handle_list(
    int sockfd,
    ChatServerState *state,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char channels[CHAT_SERVER_MAX_CHANNELS][CHAT_NAME_SIZE + 1];
    int channel_count;
    uint8_t response[SEND_PACKET_SIZE];
    size_t response_size;

    channel_count = chat_server_get_channel_names(
        state,
        channels,
        CHAT_SERVER_MAX_CHANNELS);

    response_size = chat_build_list_response(
        response,
        channels,
        (uint32_t)channel_count);

    send_packet(sockfd, response, response_size, client_addr, client_addr_len);
}

static void handle_who(
    int sockfd,
    ChatServerState *state,
    const uint8_t *packet,
    size_t packet_size,
    const struct sockaddr_storage *client_addr,
    socklen_t client_addr_len)
{
    char channel[CHAT_NAME_SIZE + 1];
    char users[CHAT_SERVER_MAX_USERS][CHAT_NAME_SIZE + 1];
    int user_count;
    uint8_t response[SEND_PACKET_SIZE];
    size_t response_size;

    if (!packet_has_size(packet_size, CHAT_WHO_REQ_SIZE))
    {
        return;
    }

    chat_read_fixed_string(channel, packet + REQ_NAME_OFFSET, CHAT_NAME_SIZE);
    debug_request(state, -1, channel, "who");

    user_count = chat_server_get_users_in_channel(
        state,
        channel,
        users,
        CHAT_SERVER_MAX_USERS);

    response_size = chat_build_who_response(
        response,
        channel,
        users,
        (uint32_t)user_count);

    send_packet(sockfd, response, response_size, client_addr, client_addr_len);
}

static void sweep_timeouts(
    ChatServerState *state,
    time_t now)
{
    int users[CHAT_SERVER_MAX_USERS];
    int user_count;
    int i;

    user_count = chat_server_collect_timed_out_users(
        state,
        now,
        SERVER_TIMEOUT_SECONDS,
        users,
        CHAT_SERVER_MAX_USERS);

    for (i = 0; i < user_count; i++)
    {
        chat_server_logout_user(state, users[i]);
    }
}

static void debug_request(
    const ChatServerState *state,
    int user_index,
    const char *channel,
    const char *message)
{
    const char *user = "unknown";

    if (state != NULL &&
        user_index >= 0 &&
        user_index < CHAT_SERVER_MAX_USERS &&
        state->users[user_index].in_use)
    {
        user = state->users[user_index].user;
    }

    printf("[%s][%s][%s]\n", channel, user, message);
    fflush(stdout);
}
