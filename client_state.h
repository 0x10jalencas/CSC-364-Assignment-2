/*
client_state.h Declares the local state helpers shared by the MustangChat
client. I track joined channels and the active channel. So commands like /join,
/leave, and /switch can be handled (outside) the main client loop.
 */

#ifndef CHAT_CLIENT_STATE_H
#define CHAT_CLIENT_STATE_H

#include "protocol.h"

#include <stdbool.h>

enum
{
    CHAT_CLIENT_MAX_CHANNELS = 64
};

typedef struct
{
    char user[CHAT_NAME_SIZE + 1];

    char joined_channels[CHAT_CLIENT_MAX_CHANNELS][CHAT_NAME_SIZE + 1];
    int joined_count;

    char active_channel[CHAT_NAME_SIZE + 1];
    bool has_active_channel;
} ChatClientState;

void chat_client_state_init(
    ChatClientState *state,
    const char *user);

bool chat_client_has_channel(
    const ChatClientState *state,
    const char *channel);

bool chat_client_join_channel(
    ChatClientState *state,
    const char *channel);

bool chat_client_leave_channel(
    ChatClientState *state,
    const char *channel);

bool chat_client_switch_channel(
    ChatClientState *state,
    const char *channel);

bool chat_client_has_active_channel(
    const ChatClientState *state);

const char *chat_client_active_channel(
    const ChatClientState *state);

#endif
