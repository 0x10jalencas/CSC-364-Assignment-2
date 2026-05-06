/*
 * protocol.c implements the MustangChat packet helpers declared in protoco
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

