/*
server_state.c Implements the user + channel state used by the MustangChat
server.

Tracks logged-in users, channel membership, empty-channel cleanup, and
soft-state timeout info.

For membership, I used this reference:

https://franckc.folk.ntnu.no/idata2302/2023/graphs/intro/c_adjacency_matrix.html

Where for faster membership, we use `bool members[CHAT_SERVER_MAX_USERS]` inside each channel.
This is basically a small, fixed membership matrix (channel x user => true/false).
Basically an adjacency matrix where a 2D table records relationships between items.
*/

#include "server_state.h"
#include "net.h"

#include <string.h>

/*
 * Private helpers
 */
static bool valid_user_index(
    const ChatServerState *state,
    int user_index);

static bool valid_channel_index(
    const ChatServerState *state,
    int channel_index);

static bool name_ok(
    const char *name);

static bool same_name(
    const char *a,
    const char *b);

static void copy_name(
    char dest[CHAT_NAME_SIZE + 1],
    const char *src);

static int find_open_user_slot(
    const ChatServerState *state);

static int find_open_channel_slot(
    const ChatServerState *state);

static int find_channel(
    const ChatServerState *state,
    const char *channel);

static int create_channel(
    ChatServerState *state,
    const char *channel);

static void delete_channel_if_empty(
    ChatServerState *state,
    int channel_index);

void chat_server_state_init(
    ChatServerState *state)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
}

int chat_server_find_user_by_addr(
    const ChatServerState *state,
    const struct sockaddr_storage *addr,
    socklen_t addr_len)
{
    int i;

    if (state == NULL || addr == NULL)
    {
        return -1;
    }

    for (i = 0; i < CHAT_SERVER_MAX_USERS; i++)
    {
        if (state->users[i].in_use &&
            chat_addr_equal(&state->users[i].addr, state->users[i].addr_len, addr, addr_len))
        {
            return i;
        }
    }

    return -1;
}

int chat_server_login_user(
    ChatServerState *state,
    const char *user,
    const struct sockaddr_storage *addr,
    socklen_t addr_len,
    time_t now)
{
    int user_index;

    if (state == NULL || !name_ok(user) || addr == NULL)
    {
        return -1;
    }

    user_index = chat_server_find_user_by_addr(state, addr, addr_len);

    if (user_index >= 0)
    {
        chat_server_logout_user(state, user_index);
    }
    else
    {
        user_index = find_open_user_slot(state);
    }

    if (user_index < 0)
    {
        return -1;
    }

    memset(&state->users[user_index], 0, sizeof(state->users[user_index]));

    state->users[user_index].in_use = true;
    copy_name(state->users[user_index].user, user);

    memcpy(&state->users[user_index].addr, addr, sizeof(*addr));
    state->users[user_index].addr_len = addr_len;

    state->users[user_index].last_seen = now;

    return user_index;
}

void chat_server_logout_user(
    ChatServerState *state,
    int user_index)
{
    int i;

    if (!valid_user_index(state, user_index))
    {
        return;
    }

    for (i = 0; i < CHAT_SERVER_MAX_CHANNELS; i++)
    {
        if (state->channels[i].in_use && state->channels[i].members[user_index])
        {
            state->channels[i].members[user_index] = false;
            state->channels[i].member_count--;
            delete_channel_if_empty(state, i);
        }
    }

    memset(&state->users[user_index], 0, sizeof(state->users[user_index]));
}

void chat_server_touch_user(
    ChatServerState *state,
    int user_index,
    time_t now)
{
    if (!valid_user_index(state, user_index))
    {
        return;
    }

    state->users[user_index].last_seen = now;
}

bool chat_server_user_in_channel(
    const ChatServerState *state,
    int user_index,
    const char *channel)
{
    int channel_index;

    if (!valid_user_index(state, user_index) || !name_ok(channel))
    {
        return false;
    }

    channel_index = find_channel(state, channel);

    if (channel_index < 0)
    {
        return false;
    }

    return state->channels[channel_index].members[user_index];
}

bool chat_server_join_channel(
    ChatServerState *state,
    int user_index,
    const char *channel)
{
    int channel_index;

    if (!valid_user_index(state, user_index) || !name_ok(channel))
    {
        return false;
    }

    channel_index = find_channel(state, channel);

    if (channel_index < 0)
    {
        channel_index = create_channel(state, channel);
    }

    if (channel_index < 0)
    {
        return false;
    }

    if (state->channels[channel_index].members[user_index])
    {
        return true;
    }

    state->channels[channel_index].members[user_index] = true;
    state->channels[channel_index].member_count++;

    return true;
}

bool chat_server_leave_channel(
    ChatServerState *state,
    int user_index,
    const char *channel)
{
    int channel_index;

    if (!valid_user_index(state, user_index) || !name_ok(channel))
    {
        return false;
    }

    channel_index = find_channel(state, channel);

    if (channel_index < 0)
    {
        return false;
    }

    if (!state->channels[channel_index].members[user_index])
    {
        return false;
    }

    state->channels[channel_index].members[user_index] = false;
    state->channels[channel_index].member_count--;

    delete_channel_if_empty(state, channel_index);

    return true;
}

int chat_server_get_channel_names(
    const ChatServerState *state,
    char channels[][CHAT_NAME_SIZE + 1],
    int max_channels)
{
    int i;
    int count = 0;

    if (state == NULL || channels == NULL || max_channels <= 0)
    {
        return 0;
    }

    for (i = 0; i < CHAT_SERVER_MAX_CHANNELS && count < max_channels; i++)
    {
        if (state->channels[i].in_use)
        {
            copy_name(channels[count], state->channels[i].name);
            count++;
        }
    }

    return count;
}

int chat_server_get_users_in_channel(
    const ChatServerState *state,
    const char *channel,
    char users[][CHAT_NAME_SIZE + 1],
    int max_users)
{
    int i;
    int channel_index;
    int count = 0;

    if (state == NULL || !name_ok(channel) || users == NULL || max_users <= 0)
    {
        return 0;
    }

    channel_index = find_channel(state, channel);

    if (channel_index < 0)
    {
        return 0;
    }

    for (i = 0; i < CHAT_SERVER_MAX_USERS && count < max_users; i++)
    {
        if (state->channels[channel_index].members[i] &&
            valid_user_index(state, i))
        {
            copy_name(users[count], state->users[i].user);
            count++;
        }
    }

    return count;
}

int chat_server_get_channel_members(
    const ChatServerState *state,
    const char *channel,
    int members[],
    int max_members)
{
    int i;
    int channel_index;
    int count = 0;

    if (state == NULL || !name_ok(channel) || members == NULL || max_members <= 0)
    {
        return 0;
    }

    channel_index = find_channel(state, channel);

    if (channel_index < 0)
    {
        return 0;
    }

    for (i = 0; i < CHAT_SERVER_MAX_USERS && count < max_members; i++)
    {
        if (state->channels[channel_index].members[i] &&
            valid_user_index(state, i))
        {
            members[count] = i;
            count++;
        }
    }

    return count;
}

int chat_server_collect_timed_out_users(
    const ChatServerState *state,
    time_t now,
    int timeout_seconds,
    int users[],
    int max_users)
{
    int i;
    int count = 0;

    if (state == NULL || users == NULL || max_users <= 0)
    {
        return 0;
    }

    for (i = 0; i < CHAT_SERVER_MAX_USERS && count < max_users; i++)
    {
        if (state->users[i].in_use &&
            now - state->users[i].last_seen >= timeout_seconds)
        {
            users[count] = i;
            count++;
        }
    }

    return count;
}

static bool valid_user_index(
    const ChatServerState *state,
    int user_index)
{
    return state != NULL &&
           user_index >= 0 &&
           user_index < CHAT_SERVER_MAX_USERS &&
           state->users[user_index].in_use;
}

static bool valid_channel_index(
    const ChatServerState *state,
    int channel_index)
{
    return state != NULL &&
           channel_index >= 0 &&
           channel_index < CHAT_SERVER_MAX_CHANNELS &&
           state->channels[channel_index].in_use;
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

static bool same_name(
    const char *a,
    const char *b)
{
    return strncmp(a, b, CHAT_NAME_SIZE + 1) == 0;
}

static void copy_name(
    char dest[CHAT_NAME_SIZE + 1],
    const char *src)
{
    int i;

    memset(dest, 0, CHAT_NAME_SIZE + 1);

    if (src == NULL)
    {
        return;
    }

    for (i = 0; i < CHAT_NAME_SIZE && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
}

static int find_open_user_slot(
    const ChatServerState *state)
{
    int i;

    if (state == NULL)
    {
        return -1;
    }

    for (i = 0; i < CHAT_SERVER_MAX_USERS; i++)
    {
        if (!state->users[i].in_use)
        {
            return i;
        }
    }

    return -1;
}

static int find_open_channel_slot(
    const ChatServerState *state)
{
    int i;

    if (state == NULL)
    {
        return -1;
    }

    for (i = 0; i < CHAT_SERVER_MAX_CHANNELS; i++)
    {
        if (!state->channels[i].in_use)
        {
            return i;
        }
    }

    return -1;
}

static int find_channel(
    const ChatServerState *state,
    const char *channel)
{
    int i;

    if (state == NULL || !name_ok(channel))
    {
        return -1;
    }

    for (i = 0; i < CHAT_SERVER_MAX_CHANNELS; i++)
    {
        if (state->channels[i].in_use && same_name(state->channels[i].name, channel))
        {
            return i;
        }
    }

    return -1;
}

static int create_channel(
    ChatServerState *state,
    const char *channel)
{
    int channel_index;

    if (state == NULL || !name_ok(channel))
    {
        return -1;
    }

    channel_index = find_open_channel_slot(state);

    if (channel_index < 0)
    {
        return -1;
    }

    memset(&state->channels[channel_index], 0, sizeof(state->channels[channel_index]));

    state->channels[channel_index].in_use = true;
    copy_name(state->channels[channel_index].name, channel);

    return channel_index;
}

static void delete_channel_if_empty(
    ChatServerState *state,
    int channel_index)
{
    if (!valid_channel_index(state, channel_index))
    {
        return;
    }

    if (state->channels[channel_index].member_count == 0)
    {
        memset(&state->channels[channel_index], 0, sizeof(state->channels[channel_index]));
    }
}
