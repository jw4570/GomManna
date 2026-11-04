#include "gommanna/room_invite.h"
#include "secure_random.h"
#include <string.h>

int gm_room_code_is_valid(const char *code) {
    size_t i;
    if (!code) return 0;
    for (i = 0; i < GM_ROOM_CODE_LENGTH; ++i) {
        if (code[i] == '\0' || !strchr(GM_ROOM_CODE_ALPHABET, code[i])) return 0;
    }
    return code[GM_ROOM_CODE_LENGTH] == '\0';
}

gm_room_status gm_room_code_generate(char *output, size_t capacity) {
    unsigned char bytes[GM_ROOM_CODE_LENGTH];
    size_t i;
    if (output && capacity) output[0] = '\0';
    if (!output) return GM_ROOM_INVALID_ARGUMENT;
    if (capacity < GM_ROOM_CODE_CAPACITY) return GM_ROOM_BUFFER_TOO_SMALL;
    if (!gm_room_random_bytes(bytes, sizeof(bytes))) return GM_ROOM_RANDOM_FAILED;
    /* 32 symbols exactly divide 256, so this mapping has no modulo bias. */
    for (i = 0; i < GM_ROOM_CODE_LENGTH; ++i) {
        output[i] = GM_ROOM_CODE_ALPHABET[bytes[i] & 31u];
    }
    output[GM_ROOM_CODE_LENGTH] = '\0';
    return GM_ROOM_OK;
}

gm_room_status gm_room_invite_payload(const char *code, char *output, size_t capacity) {
    if (output && capacity) output[0] = '\0';
    if (!output) return GM_ROOM_INVALID_ARGUMENT;
    if (!gm_room_code_is_valid(code)) return GM_ROOM_INVALID_CODE;
    if (capacity < GM_ROOM_PAYLOAD_CAPACITY) return GM_ROOM_BUFFER_TOO_SMALL;
    memcpy(output, GM_ROOM_INVITE_PREFIX, sizeof(GM_ROOM_INVITE_PREFIX) - 1);
    memcpy(output + sizeof(GM_ROOM_INVITE_PREFIX) - 1, code, GM_ROOM_CODE_CAPACITY);
    return GM_ROOM_OK;
}
