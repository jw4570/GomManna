#include "secure_random.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <limits.h>

int gm_room_random_bytes(unsigned char *output, size_t length) {
    if (length > ULONG_MAX) return 0;
    return BCryptGenRandom(NULL, output, (ULONG)length,
                          BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}
#elif defined(__linux__)
#include <errno.h>
#include <sys/random.h>

int gm_room_random_bytes(unsigned char *output, size_t length) {
    size_t done = 0;
    while (done < length) {
        ssize_t n = getrandom(output + done, length - done, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}
#else
/* macOS and other POSIX systems. No pseudo-random fallback on failure. */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int gm_room_random_bytes(unsigned char *output, size_t length) {
    int fd;
    size_t done = 0;
    do { fd = open("/dev/urandom", O_RDONLY); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return 0;
    while (done < length) {
        ssize_t n = read(fd, output + done, length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return 0; }
        done += (size_t)n;
    }
    close(fd);
    return 1;
}
#endif
