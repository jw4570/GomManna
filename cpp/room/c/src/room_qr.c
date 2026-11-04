#include "gommanna/room_invite.h"
#include "qrcodegen.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define GM_QR_MAX_VERSION 4
#define GM_QR_BORDER 4

/* Called first without an output buffer to measure the entire SVG. The second
 * pass is only called with a buffer known to be large enough. */
static int append(char *output, size_t *length, const char *format, ...) {
    char fragment[256];
    va_list args;
    int n;
    va_start(args, format);
    n = vsnprintf(fragment, sizeof(fragment), format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(fragment)) return 0;
    if (output) memcpy(output + *length, fragment, (size_t)n);
    *length += (size_t)n;
    return 1;
}

static int render(const uint8_t *qr, char *output, size_t *length) {
    int x, y;
    int side = qrcodegen_getSize(qr);
    int view = side + GM_QR_BORDER * 2;
    *length = 0;
    if (!append(output, length,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" "
        "width=\"328\" height=\"328\" shape-rendering=\"crispEdges\">"
        "<rect width=\"100%%\" height=\"100%%\" fill=\"#fff\"/>"
        "<path fill=\"#000\" d=\"", view, view)) return 0;
    for (y = 0; y < side; ++y) {
        for (x = 0; x < side; ++x) {
            if (qrcodegen_getModule(qr, x, y) &&
                !append(output, length, "M%d,%dh1v1h-1z", x + GM_QR_BORDER,
                        y + GM_QR_BORDER)) return 0;
        }
    }
    if (!append(output, length, "\"/></svg>\n")) return 0;
    if (output) output[*length] = '\0';
    return 1;
}

gm_room_status gm_room_qr_svg(const char *code, char *svg, size_t capacity,
                           size_t *required) {
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(GM_QR_MAX_VERSION)];
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(GM_QR_MAX_VERSION)];
    char payload[GM_ROOM_PAYLOAD_CAPACITY];
    size_t length;
    gm_room_status status;
    if (svg && capacity) svg[0] = '\0';
    if (required) *required = 0;
    if (!required || (!svg && capacity)) return GM_ROOM_INVALID_ARGUMENT;
    status = gm_room_invite_payload(code, payload, sizeof(payload));
    if (status != GM_ROOM_OK) return status;
    if (!qrcodegen_encodeText(payload, temp, qr, qrcodegen_Ecc_MEDIUM,
            1, GM_QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) return GM_ROOM_QR_FAILED;
    if (!render(qr, NULL, &length)) return GM_ROOM_QR_FAILED;
    *required = length + 1;
    if (!svg) return GM_ROOM_OK;
    if (capacity < *required) return GM_ROOM_BUFFER_TOO_SMALL;
    if (!render(qr, svg, &length)) {
        svg[0] = '\0';
        *required = 0;
        return GM_ROOM_QR_FAILED;
    }
    return GM_ROOM_OK;
}
