#ifndef GOMMANNA_ROOM_INVITE_H
#define GOMMANNA_ROOM_INVITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GM_ROOM_CODE_LENGTH 6
#define GM_ROOM_CODE_CAPACITY (GM_ROOM_CODE_LENGTH + 1)
#define GM_ROOM_CODE_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
#define GM_ROOM_INVITE_PREFIX "gommanna://join?code="
#define GM_ROOM_PAYLOAD_CAPACITY (sizeof(GM_ROOM_INVITE_PREFIX) + GM_ROOM_CODE_LENGTH)
/* Sufficient for every SVG produced by this module, including its final NUL. */
#define GM_ROOM_QR_SVG_CAPACITY 20000

typedef enum gm_room_status {
    GM_ROOM_OK = 0,
    GM_ROOM_INVALID_ARGUMENT,
    GM_ROOM_INVALID_CODE,
    GM_ROOM_BUFFER_TOO_SMALL,
    GM_ROOM_RANDOM_FAILED,
    GM_ROOM_QR_FAILED
} gm_room_status;

/* Generates a candidate, not a DB reservation. Retry on an atomic UNIQUE
 * conflict in the server. Uses OS secure randomness; never falls back to rand().
 * On failure, an available output buffer is set to an empty string. */
gm_room_status gm_room_code_generate(char *output, size_t capacity);

/* Strict server format: exactly six characters from GM_ROOM_CODE_ALPHABET.
 * Accepts a NUL-terminated string or NULL. No trimming/case conversion. */
int gm_room_code_is_valid(const char *code);

/* Builds the existing server contract's deep link from a saved room code.
 * All input strings must be NUL-terminated. Input/output buffers must not overlap.
 * The caller owns every buffer. Functions use no shared mutable state. */
gm_room_status gm_room_invite_payload(const char *code, char *output, size_t capacity);

/* Produces a real, scannable black/white QR SVG containing the deep link.
 * Includes a four-module white quiet zone. No scripts or external resources.
 * required must be non-NULL and receives bytes INCLUDING the terminating NUL.
 * Query: svg=NULL, capacity=0 => GM_ROOM_OK and required size, without writing.
 * A short buffer returns GM_ROOM_BUFFER_TOO_SMALL, required size, and an empty
 * string (if capacity > 0). Other errors set required=0. */
gm_room_status gm_room_qr_svg(const char *code, char *svg, size_t capacity,
                           size_t *required);

#ifdef __cplusplus
}
#endif
#endif
