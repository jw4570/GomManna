#include "gommanna/room_invite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

int main(void) {
    char code[GM_ROOM_CODE_CAPACITY + 1];
    char payload[GM_ROOM_PAYLOAD_CAPACITY + 1];
    char small[3] = {'x', 'x', 'Z'};
    char svg[GM_ROOM_QR_SVG_CAPACITY];
    size_t required = 0, actual = 0;
    char *exact;
    int i;
    const char *bad[] = {NULL, "", "ABC", "ABCDEFG", "ABCD2345", "abcd23",
                         "ABCD01", "ABCDI2", "ABCDO2", "ABC 23",
                         "ABCD2\n", "ABCD2\xFF", "<script>"};
    CHECK(strlen(GM_ROOM_CODE_ALPHABET) == 32);
    for (i = 0; i < 1000; ++i) {
        memset(code, 'Z', sizeof(code));
        CHECK(gm_room_code_generate(code, GM_ROOM_CODE_CAPACITY) == GM_ROOM_OK);
        CHECK(strlen(code) == GM_ROOM_CODE_LENGTH);
        CHECK(gm_room_code_is_valid(code));
        CHECK(code[GM_ROOM_CODE_CAPACITY] == 'Z');
    }
    CHECK(gm_room_code_generate(NULL, 9) == GM_ROOM_INVALID_ARGUMENT);
    CHECK(gm_room_code_generate(small, 2) == GM_ROOM_BUFFER_TOO_SMALL);
    CHECK(small[0] == '\0' && small[2] == 'Z');
    small[0] = 'x';
    CHECK(gm_room_code_generate(small, 0) == GM_ROOM_BUFFER_TOO_SMALL);
    CHECK(small[0] == 'x');
    CHECK(gm_room_code_is_valid("ABCD23"));
    CHECK(gm_room_code_is_valid("999999"));
    for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); ++i) {
        required = 77;
        memcpy(svg, "old data", sizeof("old data"));
        CHECK(!gm_room_code_is_valid(bad[i]));
        CHECK(gm_room_invite_payload(bad[i], payload, sizeof(payload)) == GM_ROOM_INVALID_CODE);
        CHECK(payload[0] == '\0');
        CHECK(gm_room_qr_svg(bad[i], svg, sizeof(svg), &required) == GM_ROOM_INVALID_CODE);
        CHECK(svg[0] == '\0' && required == 0);
    }
    memset(payload, 'Z', sizeof(payload));
    CHECK(gm_room_invite_payload("ABCD23", payload, GM_ROOM_PAYLOAD_CAPACITY) == GM_ROOM_OK);
    CHECK(strcmp(payload, "gommanna://join?code=ABCD23") == 0);
    CHECK(payload[GM_ROOM_PAYLOAD_CAPACITY] == 'Z');
    CHECK(gm_room_invite_payload("ABCD23", small, 2) == GM_ROOM_BUFFER_TOO_SMALL);
    CHECK(small[0] == '\0' && small[2] == 'Z');
    CHECK(gm_room_invite_payload("ABCD23", NULL, 0) == GM_ROOM_INVALID_ARGUMENT);
    CHECK(gm_room_qr_svg("ABCD23", NULL, 0, &required) == GM_ROOM_OK);
    CHECK(required > 100 && required <= GM_ROOM_QR_SVG_CAPACITY);
    CHECK(gm_room_qr_svg("ABCD23", small, 2, &actual) == GM_ROOM_BUFFER_TOO_SMALL);
    CHECK(actual == required && small[0] == '\0' && small[2] == 'Z');
    exact = (char *)malloc(required + 1);
    CHECK(exact != NULL);
    memset(exact, 'Z', required + 1);
    CHECK(gm_room_qr_svg("ABCD23", exact, required - 1, &actual) == GM_ROOM_BUFFER_TOO_SMALL);
    CHECK(actual == required && exact[0] == '\0' && exact[required - 1] == 'Z');
    CHECK(gm_room_qr_svg("ABCD23", exact, required, &actual) == GM_ROOM_OK);
    CHECK(actual == required && strlen(exact) + 1 == required && exact[required] == 'Z');
    CHECK(strncmp(exact, "<svg ", 5) == 0);
    CHECK(strstr(exact, "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>") != NULL);
    CHECK(strstr(exact, "<path fill=\"#000\" d=\"M") != NULL);
    CHECK(strstr(exact, "</svg>\n") != NULL);
    CHECK(gm_room_qr_svg("ABCD23", svg, sizeof(svg), &actual) == GM_ROOM_OK);
    CHECK(strcmp(exact, svg) == 0);
    free(exact);
    CHECK(gm_room_qr_svg("ABCD23", svg, sizeof(svg), NULL) == GM_ROOM_INVALID_ARGUMENT);
    CHECK(svg[0] == '\0');
    required = 77;
    CHECK(gm_room_qr_svg("ABCD23", NULL, 1, &required) == GM_ROOM_INVALID_ARGUMENT);
    CHECK(required == 0);
    puts("PASS: code format, OS RNG, payload, invalid input and SVG buffer boundaries");
    return 0;
}
