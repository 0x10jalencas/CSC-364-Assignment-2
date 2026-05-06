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
