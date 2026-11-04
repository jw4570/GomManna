#ifndef GM_ROOM_SECURE_RANDOM_H
#define GM_ROOM_SECURE_RANDOM_H
#include <stddef.h>
/* Internal interface; returns 1 only if all bytes were filled. */
int gm_room_random_bytes(unsigned char *output, size_t length);
#endif
