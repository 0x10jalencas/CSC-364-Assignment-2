/*
protocol.h Defines the MustangChat packet format. Specifically, its field sizes,
packet type IDs, and packet size constants shared by the client and server.

Also declares some helpers for (1) safely building, (2) reading, (3) validating
fixed-width UDP packets.
*/

#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    CHAT_U32_SIZE = 4,
    CHAT_NAME_SIZE = 32,
    CHAT_TEXT_SIZE = 64
};

/*
Client to server request type values
*/
enum
{
    CHAT_REQ_LOGIN = 0,
    CHAT_REQ_LOGOUT = 1,
    CHAT_REQ_JOIN = 2,
    CHAT_REQ_LEAVE = 3,
    CHAT_REQ_SAY = 4,
    CHAT_REQ_LIST = 5,
    CHAT_REQ_WHO = 6,
    CHAT_REQ_KEEPALIVE = 7
};

/*
Server -> client response type values
*/
enum
{
    CHAT_RESP_SAY = 0,
    CHAT_RESP_LIST = 1,
    CHAT_RESP_WHO = 2,
    CHAT_RESP_ERROR = 3
};

/*
Fixed client -> server request sizes
*/
enum
{
    CHAT_LOGIN_REQ_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    CHAT_LOGOUT_REQ_SIZE = CHAT_U32_SIZE,
    CHAT_JOIN_REQ_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    CHAT_LEAVE_REQ_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    CHAT_SAY_REQ_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE + CHAT_TEXT_SIZE,
    CHAT_LIST_REQ_SIZE = CHAT_U32_SIZE,
    CHAT_WHO_REQ_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE,
    CHAT_KEEPALIVE_REQ_SIZE = CHAT_U32_SIZE
};

/*
Server to client response sizes
*/
enum
{
    CHAT_SAY_RESP_SIZE = CHAT_U32_SIZE + CHAT_NAME_SIZE + CHAT_NAME_SIZE + CHAT_TEXT_SIZE,
    CHAT_ERROR_RESP_SIZE = CHAT_U32_SIZE + CHAT_TEXT_SIZE,
    CHAT_LIST_RESP_BASE_SIZE = CHAT_U32_SIZE + CHAT_U32_SIZE,
    CHAT_WHO_RESP_BASE_SIZE = CHAT_U32_SIZE + CHAT_U32_SIZE + CHAT_NAME_SIZE
};

/*
Raw integer helpers
*/
void chat_write_u32(uint8_t *buf, uint32_t value);
uint32_t chat_read_u32(const uint8_t *buf);

/*
Fixed-width string helpers
*/
void chat_write_fixed_string(
    uint8_t *dest,
    const char *src,
    size_t field_size);

void chat_read_fixed_string(
    char *dest,
    const uint8_t *src,
    size_t field_size);

/*
Validation helpers.
*/
bool chat_is_request_type(uint32_t type);
bool chat_is_response_type(uint32_t type);

size_t chat_request_size(uint32_t type);

/*
Client to server request builders
*/
size_t chat_build_login_request(
    uint8_t *buf,
    const char *username);

size_t chat_build_logout_request(
    uint8_t *buf);

size_t chat_build_join_request(
    uint8_t *buf,
    const char *channel);

size_t chat_build_leave_request(
    uint8_t *buf,
    const char *channel);

size_t chat_build_say_request(
    uint8_t *buf,
    const char *channel,
    const char *text);

size_t chat_build_list_request(
    uint8_t *buf);

size_t chat_build_who_request(
    uint8_t *buf,
    const char *channel);

size_t chat_build_keepalive_request(
    uint8_t *buf);

/*
Server to client response builders
*/
size_t chat_build_say_response(
    uint8_t *buf,
    const char *channel,
    const char *username,
    const char *text);

size_t chat_build_error_response(
    uint8_t *buf,
    const char *message);

size_t chat_build_list_response(
    uint8_t *buf,
    const char channels[][CHAT_NAME_SIZE + 1],
    uint32_t channel_count);

size_t chat_build_who_response(
    uint8_t *buf,
    const char *channel,
    const char users[][CHAT_NAME_SIZE + 1],
    uint32_t user_count);

#endif