/*
protocol.c Implements the MustangChat packet helpers declared in protocol.h
*/

#include "protocol.h"

#include <arpa/inet.h>
#include <string.h>

enum
{
    TYPE_OFFSET = 0,
    FIELD_OFFSET = CHAT_U32_SIZE,

    SAY_REQ_CHANNEL_OFFSET = CHAT_U32_SIZE,
    SAY_REQ_TEXT_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE,

    SAY_RESP_CHANNEL_OFFSET = CHAT_U32_SIZE,
    SAY_RESP_USER_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    SAY_RESP_TEXT_OFFSET = CHAT_U32_SIZE + CHAT_NAME_SIZE + CHAT_NAME_SIZE,

    COUNT_OFFSET = CHAT_U32_SIZE,
    WHO_RESP_CHANNEL_OFFSET = CHAT_U32_SIZE + CHAT_U32_SIZE
};

/*
* Private helpers
*/
static size_t bounded_strlen(const char *s, size_t limit);

static size_t build_type_request(
    uint8_t *packet,
    uint32_t type);

static size_t build_named_request(
    uint8_t *packet,
    uint32_t type,
    const char *name);

static size_t write_fixed_name_fields(
    uint8_t *packet,
    size_t offset,
    const char names[][CHAT_NAME_SIZE + 1],
    uint32_t count);

void chat_write_u32(uint8_t *packet, uint32_t value)
{
    uint32_t network_value = htonl(value);

    memcpy(packet, &network_value, CHAT_U32_SIZE);
}

uint32_t chat_read_u32(const uint8_t *packet)
{
    uint32_t network_value = 0;

    memcpy(&network_value, packet, CHAT_U32_SIZE);

    return ntohl(network_value);
}

void chat_write_fixed_string(
    uint8_t *dest,
    const char *src,
    size_t field_size)
{
    size_t len = bounded_strlen(src, field_size);

    memset(dest, 0, field_size);

    if (len > 0)
    {
        memcpy(dest, src, len);
    }
}

void chat_read_fixed_string(
    char *dest,
    const uint8_t *src,
    size_t field_size)
{
    memcpy(dest, src, field_size);
    dest[field_size] = '\0';
}

bool chat_is_request_type(uint32_t type)
{
    switch (type)
    {
    case CHAT_REQ_LOGIN:
    case CHAT_REQ_LOGOUT:
    case CHAT_REQ_JOIN:
    case CHAT_REQ_LEAVE:
    case CHAT_REQ_SAY:
    case CHAT_REQ_LIST:
    case CHAT_REQ_WHO:
    case CHAT_REQ_KEEPALIVE:
        return true;

    default:
        return false;
    }
}

bool chat_is_response_type(uint32_t type)
{
    switch (type)
    {
    case CHAT_RESP_SAY:
    case CHAT_RESP_LIST:
    case CHAT_RESP_WHO:
    case CHAT_RESP_ERROR:
        return true;

    default:
        return false;
    }
}

size_t chat_request_size(uint32_t type)
{
    switch (type)
    {
    case CHAT_REQ_LOGIN:
        return CHAT_LOGIN_REQ_SIZE;

    case CHAT_REQ_LOGOUT:
        return CHAT_LOGOUT_REQ_SIZE;

    case CHAT_REQ_JOIN:
        return CHAT_JOIN_REQ_SIZE;

    case CHAT_REQ_LEAVE:
        return CHAT_LEAVE_REQ_SIZE;

    case CHAT_REQ_SAY:
        return CHAT_SAY_REQ_SIZE;

    case CHAT_REQ_LIST:
        return CHAT_LIST_REQ_SIZE;

    case CHAT_REQ_WHO:
        return CHAT_WHO_REQ_SIZE;

    case CHAT_REQ_KEEPALIVE:
        return CHAT_KEEPALIVE_REQ_SIZE;

    default:
        return 0;
    }
}

size_t chat_build_login_request(
    uint8_t *packet,
    const char *user)
{
    return build_named_request(packet, CHAT_REQ_LOGIN, user);
}

size_t chat_build_logout_request(uint8_t *packet)
{
    return build_type_request(packet, CHAT_REQ_LOGOUT);
}

size_t chat_build_join_request(
    uint8_t *packet,
    const char *channel)
{
    return build_named_request(packet, CHAT_REQ_JOIN, channel);
}

size_t chat_build_leave_request(
    uint8_t *packet,
    const char *channel)
{
    return build_named_request(packet, CHAT_REQ_LEAVE, channel);
}

size_t chat_build_say_request(
    uint8_t *packet,
    const char *channel,
    const char *text)
{
    chat_write_u32(packet + TYPE_OFFSET, CHAT_REQ_SAY);
    chat_write_fixed_string(packet + SAY_REQ_CHANNEL_OFFSET, channel, CHAT_NAME_SIZE);
    chat_write_fixed_string(packet + SAY_REQ_TEXT_OFFSET, text, CHAT_TEXT_SIZE);

    return CHAT_SAY_REQ_SIZE;
}

size_t chat_build_list_request(uint8_t *packet)
{
    return build_type_request(packet, CHAT_REQ_LIST);
}

size_t chat_build_who_request(
    uint8_t *packet,
    const char *channel)
{
    return build_named_request(packet, CHAT_REQ_WHO, channel);
}

size_t chat_build_keepalive_request(uint8_t *packet)
{
    return build_type_request(packet, CHAT_REQ_KEEPALIVE);
}

size_t chat_build_say_response(
    uint8_t *packet,
    const char *channel,
    const char *user,
    const char *text)
{
    chat_write_u32(packet + TYPE_OFFSET, CHAT_RESP_SAY);
    chat_write_fixed_string(packet + SAY_RESP_CHANNEL_OFFSET, channel, CHAT_NAME_SIZE);
    chat_write_fixed_string(packet + SAY_RESP_USER_OFFSET, user, CHAT_NAME_SIZE);
    chat_write_fixed_string(packet + SAY_RESP_TEXT_OFFSET, text, CHAT_TEXT_SIZE);

    return CHAT_SAY_RESP_SIZE;
}

size_t chat_build_error_response(
    uint8_t *packet,
    const char *message)
{
    chat_write_u32(packet + TYPE_OFFSET, CHAT_RESP_ERROR);
    chat_write_fixed_string(packet + FIELD_OFFSET, message, CHAT_TEXT_SIZE);

    return CHAT_ERROR_RESP_SIZE;
}

size_t chat_build_list_response(
    uint8_t *packet,
    const char channels[][CHAT_NAME_SIZE + 1],
    uint32_t channel_count)
{
    chat_write_u32(packet + TYPE_OFFSET, CHAT_RESP_LIST);
    chat_write_u32(packet + COUNT_OFFSET, channel_count);

    return write_fixed_name_fields(packet, CHAT_LIST_RESP_BASE_SIZE, channels, channel_count);
}

size_t chat_build_who_response(
    uint8_t *packet,
    const char *channel,
    const char users[][CHAT_NAME_SIZE + 1],
    uint32_t user_count)
{
    chat_write_u32(packet + TYPE_OFFSET, CHAT_RESP_WHO);
    chat_write_u32(packet + COUNT_OFFSET, user_count);
    chat_write_fixed_string(packet + WHO_RESP_CHANNEL_OFFSET, channel, CHAT_NAME_SIZE);

    return write_fixed_name_fields(packet, CHAT_WHO_RESP_BASE_SIZE, users, user_count);
}

static size_t bounded_strlen(const char *s, size_t limit)
{
    size_t len = 0;

    if (s == NULL)
    {
        return 0;
    }

    while (len < limit && s[len] != '\0')
    {
        len++;
    }

    return len;
}

static size_t build_type_request(
    uint8_t *packet,
    uint32_t type)
{
    chat_write_u32(packet + TYPE_OFFSET, type);

    return CHAT_U32_SIZE;
}

static size_t build_named_request(
    uint8_t *packet,
    uint32_t type,
    const char *name)
{
    chat_write_u32(packet + TYPE_OFFSET, type);
    chat_write_fixed_string(packet + FIELD_OFFSET, name, CHAT_NAME_SIZE);

    return CHAT_U32_SIZE + CHAT_NAME_SIZE;
}

static size_t write_fixed_name_fields(
    uint8_t *packet,
    size_t offset,
    const char names[][CHAT_NAME_SIZE + 1],
    uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
    {
        chat_write_fixed_string(packet + offset, names[i], CHAT_NAME_SIZE);
        offset += CHAT_NAME_SIZE;
    }

    return offset;
}