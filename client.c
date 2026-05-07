/*
client.c Implements the MustangChat client. 

Connects to the server, tracks user's local channel state, sends client
requests, and displays server responses.
 */

#include "client_state.h"
#include "net.h"
#include "protocol.h"

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
    INPUT_SIZE = 1024,
    RECV_PACKET_SIZE = 4096,
    KEEPALIVE_SECONDS = 60,

    RESP_COUNT_OFFSET = CHAT_U32_SIZE,

    RESP_SAY_CHANNEL_OFFSET = CHAT_U32_SIZE,
    RESP_SAY_USER_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    RESP_SAY_TEXT_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE + CHAT_NAME_SIZE,

    RESP_ERROR_TEXT_OFFSET = CHAT_U32_SIZE,

    RESP_LIST_NAMES_OFFSET = CHAT_LIST_RESP_BASE_SIZE,

    RESP_WHO_CHANNEL_OFFSET = CHAT_U32_SIZE + CHAT_U32_SIZE,
    RESP_WHO_USERS_OFFSET = CHAT_WHO_RESP_BASE_SIZE
};

/*
 * Private helpers
 */
static void usage(
    const char *program);

static void prompt(void);

static void strip_newline(
    char *line);

static void flush_line(void);

static bool name_ok(
    const char *name);

static bool parse_channel_arg(
    const char *line,
    const char *command,
    char channel[CHAT_NAME_SIZE + 1]);

static void send_packet(
    int sockfd,
    const uint8_t *packet,
    size_t packet_size,
    time_t *last_sent);

static void handle_user_line(
    int sockfd,
    ChatClientState *state,
    char *line,
    bool *running,
    time_t *last_sent);

static void handle_server_packet(
    const uint8_t *packet,
    size_t packet_size);

static void maybe_send_keepalive(
    int sockfd,
    time_t *last_sent);

int main(
    int argc,
    char **argv)
{
    const char *host;
    const char *port;
    const char *user;
    ChatClientState state;
    int sockfd;
    bool running = true;
    time_t last_sent = 0;

    if (argc != 4)
    {
        usage(argv[0]);
        return 1;
    }

    host = argv[1];
    port = argv[2];
    user = argv[3];

    if (!name_ok(user))
    {
        fprintf(stderr, "Invalid username\n");
        return 1;
    }

    sockfd = chat_create_client_socket(host, port);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    chat_client_state_init(&state, user);

    {
        uint8_t packet[CHAT_LOGIN_REQ_SIZE];

        send_packet(
            sockfd,
            packet,
            chat_build_login_request(packet, user),
            &last_sent);
    }

    {
        uint8_t packet[CHAT_JOIN_REQ_SIZE];

        send_packet(
            sockfd,
            packet,
            chat_build_join_request(packet, "Common"),
            &last_sent);
    }

    chat_client_join_channel(&state, "Common");
    prompt();

    while (running)
    {
        fd_set readfds;
        struct timeval timeout;
        int maxfd;
        int ready;
        bool print_after_event = false;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ready = select(maxfd + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0)
        {
            perror("select");
            break;
        }

        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds))
        {
            char line[INPUT_SIZE];

            if (fgets(line, sizeof(line), stdin) == NULL)
            {
                uint8_t packet[CHAT_LOGOUT_REQ_SIZE];

                send_packet(
                    sockfd,
                    packet,
                    chat_build_logout_request(packet),
                    &last_sent);

                running = false;
            }
            else
            {
                if (strchr(line, '\n') == NULL)
                {
                    flush_line();
                }

                strip_newline(line);
                handle_user_line(sockfd, &state, line, &running, &last_sent);
                print_after_event = true;
            }
        }

        if (running && ready > 0 && FD_ISSET(sockfd, &readfds))
        {
            uint8_t packet[RECV_PACKET_SIZE];
            ssize_t packet_size;

            packet_size = recv(sockfd, packet, sizeof(packet), 0);

            if (packet_size > 0)
            {
                handle_server_packet(packet, (size_t)packet_size);
                print_after_event = true;
            }
        }

        if (running)
        {
            maybe_send_keepalive(sockfd, &last_sent);

            if (print_after_event)
            {
                prompt();
            }
        }
    }

    close(sockfd);

    return 0;
}

static void usage(
    const char *program)
{
    fprintf(stderr, "Usage: %s <host> <port> <username>\n", program);
}

static void prompt(void)
{
    printf("> ");
    fflush(stdout);
}

static void strip_newline(
    char *line)
{
    size_t len;

    if (line == NULL)
    {
        return;
    }

    len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[len - 1] = '\0';
        len--;
    }
}

static void flush_line(void)
{
    int ch;

    do
    {
        ch = getchar();
    } while (ch != '\n' && ch != EOF);
}

static bool name_ok(
    const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0')
    {
        return false;
    }

    for (i = 0; i <= CHAT_NAME_SIZE; i++)
    {
        if (name[i] == '\0')
        {
            return true;
        }
    }

    return false;
}

static bool parse_channel_arg(
    const char *line,
    const char *command,
    char channel[CHAT_NAME_SIZE + 1])
{
    char extra;
    int matched;
    size_t command_len;

    memset(channel, 0, CHAT_NAME_SIZE + 1);

    command_len = strlen(command);

    if (strncmp(line, command, command_len) != 0)
    {
        return false;
    }

    if (line[command_len] != ' ' && line[command_len] != '\t')
    {
        return false;
    }

    matched = sscanf(line + command_len, " %32s %c", channel, &extra);

    return matched == 1 && name_ok(channel);
}

static void send_packet(
    int sockfd,
    const uint8_t *packet,
    size_t packet_size,
    time_t *last_sent)
{
    if (send(sockfd, packet, packet_size, 0) < 0)
    {
        perror("send");
        return;
    }

    if (last_sent != NULL)
    {
        *last_sent = time(NULL);
    }
}

static void handle_user_line(
    int sockfd,
    ChatClientState *state,
    char *line,
    bool *running,
    time_t *last_sent)
{
    char channel[CHAT_NAME_SIZE + 1];
    uint8_t packet[CHAT_SAY_REQ_SIZE];
    size_t packet_size;

    if (line[0] == '\0')
    {
        return;
    }

    if (strcmp(line, "/exit") == 0)
    {
        packet_size = chat_build_logout_request(packet);
        send_packet(sockfd, packet, packet_size, last_sent);
        *running = false;
        return;
    }

    if (parse_channel_arg(line, "/join", channel))
    {
        if (chat_client_join_channel(state, channel))
        {
            packet_size = chat_build_join_request(packet, channel);
            send_packet(sockfd, packet, packet_size, last_sent);
        }

        return;
    }

    if (parse_channel_arg(line, "/leave", channel))
    {
        if (!chat_client_has_channel(state, channel))
        {
            printf("Error: you are not subscribed to channel %s\n", channel);
            return;
        }

        packet_size = chat_build_leave_request(packet, channel);
        send_packet(sockfd, packet, packet_size, last_sent);
        chat_client_leave_channel(state, channel);
        return;
    }

    if (strcmp(line, "/list") == 0)
    {
        packet_size = chat_build_list_request(packet);
        send_packet(sockfd, packet, packet_size, last_sent);
        return;
    }

    if (parse_channel_arg(line, "/who", channel))
    {
        packet_size = chat_build_who_request(packet, channel);
        send_packet(sockfd, packet, packet_size, last_sent);
        return;
    }

    if (parse_channel_arg(line, "/switch", channel))
    {
        if (!chat_client_switch_channel(state, channel))
        {
            printf("Error: you are not subscribed to channel %s\n", channel);
        }

        return;
    }

    if (line[0] == '/')
    {
        printf("Unknown command\n");
        return;
    }

    if (!chat_client_has_active_channel(state))
    {
        return;
    }

    packet_size = chat_build_say_request(
        packet,
        chat_client_active_channel(state),
        line);

    send_packet(sockfd, packet, packet_size, last_sent);
}

static void handle_server_packet(
    const uint8_t *packet,
    size_t packet_size)
{
    uint32_t type;

    if (packet_size < CHAT_U32_SIZE)
    {
        return;
    }

    type = chat_read_u32(packet);

    if (type == CHAT_RESP_SAY && packet_size >= CHAT_SAY_RESP_SIZE)
    {
        char channel[CHAT_NAME_SIZE + 1];
        char user[CHAT_NAME_SIZE + 1];
        char text[CHAT_TEXT_SIZE + 1];

        chat_read_fixed_string(channel, packet + RESP_SAY_CHANNEL_OFFSET, CHAT_NAME_SIZE);
        chat_read_fixed_string(user, packet + RESP_SAY_USER_OFFSET, CHAT_NAME_SIZE);
        chat_read_fixed_string(text, packet + RESP_SAY_TEXT_OFFSET, CHAT_TEXT_SIZE);

        printf("\n[%s][%s]: %s\n", channel, user, text);
    }
    else if (type == CHAT_RESP_LIST && packet_size >= CHAT_LIST_RESP_BASE_SIZE)
    {
        uint32_t channel_count;
        uint32_t i;
        size_t offset = RESP_LIST_NAMES_OFFSET;

        channel_count = chat_read_u32(packet + RESP_COUNT_OFFSET);

        if (channel_count > (packet_size - CHAT_LIST_RESP_BASE_SIZE) / CHAT_NAME_SIZE)
        {
            return;
        }

        printf("\nExisting channels:\n");

        for (i = 0; i < channel_count; i++)
        {
            char channel[CHAT_NAME_SIZE + 1];

            chat_read_fixed_string(channel, packet + offset, CHAT_NAME_SIZE);
            printf("  %s\n", channel);

            offset += CHAT_NAME_SIZE;
        }
    }
    else if (type == CHAT_RESP_WHO && packet_size >= CHAT_WHO_RESP_BASE_SIZE)
    {
        uint32_t user_count;
        uint32_t i;
        size_t offset = RESP_WHO_USERS_OFFSET;
        char channel[CHAT_NAME_SIZE + 1];

        user_count = chat_read_u32(packet + RESP_COUNT_OFFSET);

        if (user_count > (packet_size - CHAT_WHO_RESP_BASE_SIZE) / CHAT_NAME_SIZE)
        {
            return;
        }

        chat_read_fixed_string(channel, packet + RESP_WHO_CHANNEL_OFFSET, CHAT_NAME_SIZE);

        printf("\nUsers on channel %s:\n", channel);

        for (i = 0; i < user_count; i++)
        {
            char user[CHAT_NAME_SIZE + 1];

            chat_read_fixed_string(user, packet + offset, CHAT_NAME_SIZE);
            printf("  %s\n", user);

            offset += CHAT_NAME_SIZE;
        }
    }
    else if (type == CHAT_RESP_ERROR && packet_size >= CHAT_ERROR_RESP_SIZE)
    {
        char message[CHAT_TEXT_SIZE + 1];

        chat_read_fixed_string(message, packet + RESP_ERROR_TEXT_OFFSET, CHAT_TEXT_SIZE);
        printf("\nError: %s\n", message);
    }
}

static void maybe_send_keepalive(
    int sockfd,
    time_t *last_sent)
{
    time_t now;
    uint8_t packet[CHAT_KEEPALIVE_REQ_SIZE];
    size_t packet_size;

    if (last_sent == NULL)
    {
        return;
    }

    now = time(NULL);

    if (now - *last_sent < KEEPALIVE_SECONDS)
    {
        return;
    }

    packet_size = chat_build_keepalive_request(packet);
    send_packet(sockfd, packet, packet_size, last_sent);
}
