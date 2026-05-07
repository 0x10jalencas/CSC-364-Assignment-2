/*
client_state.c Implements the local channel state used by the MustangChat
client. Handles switching channels, which is separate from networking and packet
formatting.
 */

#include "client_state.h"

#include <string.h>

/*
 * Private helpers
 */
static bool name_ok(const char *name);

static bool same_name(
    const char *a,
    const char *b);

static int find_channel_index(
    const ChatClientState *state,
    const char *channel);

static void copy_name(
    char dest[CHAT_NAME_SIZE + 1],
    const char *src);

static void clear_name(
    char name[CHAT_NAME_SIZE + 1]);

static bool active_channel_is(
    const ChatClientState *state,
    const char *channel);

void chat_client_state_init(
    ChatClientState *state,
    const char *user)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    copy_name(state->user, user);
}

bool chat_client_has_channel(
    const ChatClientState *state,
    const char *channel)
{
    return find_channel_index(state, channel) >= 0;
}

bool chat_client_join_channel(
    ChatClientState *state,
    const char *channel)
{
    if (state == NULL || !name_ok(channel))
    {
        return false;
    }

    if (chat_client_has_channel(state, channel))
    {
        copy_name(state->active_channel, channel);
        state->has_active_channel = true;

        return true;
    }

    if (state->joined_count >= CHAT_CLIENT_MAX_CHANNELS)
    {
        return false;
    }

    copy_name(state->joined_channels[state->joined_count], channel);
    state->joined_count++;

    copy_name(state->active_channel, channel);
    state->has_active_channel = true;

    return true;
}

bool chat_client_leave_channel(
    ChatClientState *state,
    const char *channel)
{
    int index;
    int channels_after;

    if (state == NULL || !name_ok(channel))
    {
        return false;
    }

    index = find_channel_index(state, channel);

    if (index < 0)
    {
        return false;
    }

    channels_after = state->joined_count - index - 1;

    if (channels_after > 0)
    {
        memmove(
            state->joined_channels[index],
            state->joined_channels[index + 1],
            (size_t)channels_after * sizeof(state->joined_channels[0]));
    }

    state->joined_count--;
    clear_name(state->joined_channels[state->joined_count]);

    if (active_channel_is(state, channel))
    {
        clear_name(state->active_channel);
        state->has_active_channel = false;
    }

    return true;
}

bool chat_client_switch_channel(
    ChatClientState *state,
    const char *channel)
{
    if (state == NULL || !name_ok(channel))
    {
        return false;
    }

    if (!chat_client_has_channel(state, channel))
    {
        return false;
    }

    copy_name(state->active_channel, channel);
    state->has_active_channel = true;

    return true;
}

bool chat_client_has_active_channel(
    const ChatClientState *state)
{
    return state != NULL && state->has_active_channel;
}

const char *chat_client_active_channel(
    const ChatClientState *state)
{
    if (!chat_client_has_active_channel(state))
    {
        return NULL;
    }

    return state->active_channel;
}

static bool name_ok(const char *name)
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

static bool same_name(
    const char *a,
    const char *b)
{
    return strncmp(a, b, CHAT_NAME_SIZE + 1) == 0;
}

static int find_channel_index(
    const ChatClientState *state,
    const char *channel)
{
    int i;

    if (state == NULL || !name_ok(channel))
    {
        return -1;
    }

    for (i = 0; i < state->joined_count; i++)
    {
        if (same_name(state->joined_channels[i], channel))
        {
            return i;
        }
    }

    return -1;
}

static void copy_name(
    char dest[CHAT_NAME_SIZE + 1],
    const char *src)
{
    int i;

    clear_name(dest);

    if (src == NULL)
    {
        return;
    }

    for (i = 0; i < CHAT_NAME_SIZE && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
}

static void clear_name(
    char name[CHAT_NAME_SIZE + 1])
{
    memset(name, 0, CHAT_NAME_SIZE + 1);
}

static bool active_channel_is(
    const ChatClientState *state,
    const char *channel)
{
    return state->has_active_channel && same_name(state->active_channel, channel);
}
