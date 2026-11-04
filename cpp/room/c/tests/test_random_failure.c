#include "gommanna/room_invite.h"
#include "secure_random.h"
#include <stdio.h>
#include <string.h>

static int succeed = 0;
int gm_room_random_bytes(unsigned char *output, size_t length) {
    const unsigned char bytes[] = {0, 31, 32, 63, 64, 95, 128, 255};
    if (length != sizeof(bytes)) return 0;
    memcpy(output, bytes, sizeof(bytes));
    return succeed;
}

int main(void) {
    char code[GM_ROOM_CODE_CAPACITY] = "OLD23456";
    if (gm_room_code_generate(code, sizeof(code)) != GM_ROOM_RANDOM_FAILED || code[0]) {
        fputs("FAIL: RNG failure must not expose a partially generated code\n", stderr);
        return 1;
    }
    succeed = 1;
    if (gm_room_code_generate(code, sizeof(code)) != GM_ROOM_OK || strcmp(code, "A9A9A9A9")) {
        fputs("FAIL: full random byte range must map to valid code characters\n", stderr);
        return 1;
    }
    puts("PASS: RNG failure propagation and byte mapping boundaries");
    return 0;
}
