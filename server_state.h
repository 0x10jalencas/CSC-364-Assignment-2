/*
server_state.h Declares the user + channel state helpers used by the MustangChat
server.

Membership, channel cleanup, and timeout tracking are once again separate from
packet handling.
 */

#ifndef CHAT_SERVER_STATE_H
#define CHAT_SERVER_STATE_H

#include "protocol.h"

#include <stdbool.h>
#include <sys/socket.h>
#include <time.h>

enum
{
    CHAT_SERVER_MAX_USERS = 64,
    CHAT_SERVER_MAX_CHANNELS = 64
};

typedef struct
{
    bool in_use;

    char user[CHAT_NAME_SIZE + 1];

    struct sockaddr_storage addr;
    socklen_t addr_len;

    time_t last_seen;
} ChatServerUser;

typedef struct
{
    bool in_use;

    char name[CHAT_NAME_SIZE + 1];

    bool members[CHAT_SERVER_MAX_USERS];
    int member_count;
} ChatServerChannel;

typedef struct
{
    ChatServerUser users[CHAT_SERVER_MAX_USERS];
    ChatServerChannel channels[CHAT_SERVER_MAX_CHANNELS];
} ChatServerState;

void chat_server_state_init(
    ChatServerState *state);

int chat_server_find_user_by_addr(
    const ChatServerState *state,
    const struct sockaddr_storage *addr,
    socklen_t addr_len);

int chat_server_login_user(
    ChatServerState *state,
    const char *user,
    const struct sockaddr_storage *addr,
    socklen_t addr_len,
    time_t now);

void chat_server_logout_user(
    ChatServerState *state,
    int user_index);

void chat_server_touch_user(
    ChatServerState *state,
    int user_index,
    time_t now);

bool chat_server_user_in_channel(
    const ChatServerState *state,
    int user_index,
    const char *channel);

bool chat_server_join_channel(
    ChatServerState *state,
    int user_index,
    const char *channel);

bool chat_server_leave_channel(
    ChatServerState *state,
    int user_index,
    const char *channel);

int chat_server_get_channel_names(
    const ChatServerState *state,
    char channels[][CHAT_NAME_SIZE + 1],
    int max_channels);

int chat_server_get_users_in_channel(
    const ChatServerState *state,
    const char *channel,
    char users[][CHAT_NAME_SIZE + 1],
    int max_users);

int chat_server_get_channel_members(
    const ChatServerState *state,
    const char *channel,
    int members[],
    int max_members);

int chat_server_collect_timed_out_users(
    const ChatServerState *state,
    time_t now,
    int timeout_seconds,
    int users[],
    int max_users);

#endif