#include "gommanna/room_invite.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char code[GM_ROOM_CODE_CAPACITY];
    char payload[GM_ROOM_PAYLOAD_CAPACITY];
    char svg[GM_ROOM_QR_SVG_CAPACITY];
    size_t required;
    gm_room_status status;
    if (argc > 2) {
        fprintf(stderr, "Usage: room_invite_example [6-character-code]\n");
        return 2;
    }
    if (argc == 2) {
        if (!gm_room_code_is_valid(argv[1])) {
            fprintf(stderr, "Invalid code: use 6 characters from %s\n", GM_ROOM_CODE_ALPHABET);
            return 2;
        }
        memcpy(code, argv[1], sizeof(code));
    } else {
        status = gm_room_code_generate(code, sizeof(code));
        if (status != GM_ROOM_OK) {
            fprintf(stderr, "Code generation failed: %d\n", (int)status);
            return 1;
        }
    }
    if (gm_room_invite_payload(code, payload, sizeof(payload)) != GM_ROOM_OK ||
        gm_room_qr_svg(code, svg, sizeof(svg), &required) != GM_ROOM_OK) {
        fprintf(stderr, "QR generation failed\n");
        return 1;
    }
    fprintf(stderr, "invite_code=%s\nqr_payload=%s\n", code, payload);
    if (fwrite(svg, 1, required - 1, stdout) != required - 1 || fflush(stdout) != 0) return 1;
    return 0;
}
