/*
 * libc.c
 *
 * Bionic-compatible replacements for libc bits that differ from newlib:
 * the __sF standard streams, fpclassify constants, utsname, mbstate_t, etc.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "reimpl/libc.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>

#include "game/context.h"
#include "utils/logger.h"

/*
 * Standard streams
 *
 * The game's `stdin/stdout/stderr` are `&__sF[0..2]` inside bionic's own
 * array of 84-byte FILE structs. We give it a dummy array of the right size
 * and translate those pointers in every stdio import that takes a FILE*.
 * Output to stdout/stderr ends up in the loader's log.
 */

uint8_t __sF_fake[3][BIONIC_FILE_SIZE];

static char *environ_empty[] = { NULL };
char **environ_fake = environ_empty;

static inline int std_index(FILE *f) {
    uintptr_t p = (uintptr_t)f, base = (uintptr_t)__sF_fake;
    if (p >= base && p < base + sizeof(__sF_fake))
        return (int)((p - base) / BIONIC_FILE_SIZE);
    return -1;
}

static void std_write(const char *s, size_t len) {
    // The game's screen changes are also visible in its stdout logging
    char line[512];
    size_t n = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
    memcpy(line, s, n);
    line[n] = '\0';
    context_feed(line);

#ifdef DEBUG_SOLOADER
    sceClibPrintf("[GAME] %.*s", (int)len, s);
    if (len && s[len - 1] != '\n')
        sceClibPrintf("\n");
#endif
}

int fclose_sf(FILE *f) {
    return std_index(f) >= 0 ? 0 : fclose(f);
}

int fflush_sf(FILE *f) {
    if (!f) return fflush(NULL);
    return std_index(f) >= 0 ? 0 : fflush(f);
}

int fgetc_sf(FILE *f) {
    return std_index(f) >= 0 ? EOF : fgetc(f);
}

char *fgets_sf(char *s, int n, FILE *f) {
    return std_index(f) >= 0 ? NULL : fgets(s, n, f);
}

int vfprintf_sf(FILE *f, const char *fmt, va_list ap) {
    if (std_index(f) >= 0) {
        char buf[1024];
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        if (n > 0) std_write(buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
        return n;
    }
    return vfprintf(f, fmt, ap);
}

int fprintf_sf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf_sf(f, fmt, ap);
    va_end(ap);
    return ret;
}

int fputc_sf(int c, FILE *f) {
    if (std_index(f) >= 0) {
        char ch = (char)c;
        std_write(&ch, 1);
        return c & 0xFF;
    }
    return fputc(c, f);
}

int fputs_sf(const char *s, FILE *f) {
    if (std_index(f) >= 0) {
        std_write(s, strlen(s));
        return 0;
    }
    return fputs(s, f);
}

size_t fread_sf(void *ptr, size_t size, size_t n, FILE *f) {
    return std_index(f) >= 0 ? 0 : fread(ptr, size, n, f);
}

size_t fwrite_sf(const void *ptr, size_t size, size_t n, FILE *f) {
    if (std_index(f) >= 0) {
        std_write(ptr, size * n);
        return n;
    }
    return fwrite(ptr, size, n, f);
}

int fseek_sf(FILE *f, long off, int whence) {
    return std_index(f) >= 0 ? -1 : fseek(f, off, whence);
}

long ftell_sf(FILE *f) {
    return std_index(f) >= 0 ? -1 : ftell(f);
}

void rewind_sf(FILE *f) {
    if (std_index(f) < 0) rewind(f);
}

int setvbuf_sf(FILE *f, char *buf, int mode, size_t size) {
    return std_index(f) >= 0 ? 0 : setvbuf(f, buf, mode, size);
}

int ungetc_sf(int c, FILE *f) {
    return std_index(f) >= 0 ? EOF : ungetc(c, f);
}

wint_t getwc_sf(FILE *f) {
    return std_index(f) >= 0 ? WEOF : getwc(f);
}

wint_t putwc_sf(wchar_t c, FILE *f) {
    if (std_index(f) >= 0) {
        char ch = (char)c;
        std_write(&ch, 1);
        return (wint_t)c;
    }
    return putwc(c, f);
}

wint_t ungetwc_sf(wint_t c, FILE *f) {
    return std_index(f) >= 0 ? WEOF : ungetwc(c, f);
}

int vprintf_soloader(const char *fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) std_write(buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
    return n;
}

int printf_soloader(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf_soloader(fmt, ap);
    va_end(ap);
    return ret;
}

int puts_soloader(const char *s) {
    std_write(s, strlen(s));
    std_write("\n", 1);
    return 1;
}

int putchar_soloader(int c) {
    char ch = (char)c;
    std_write(&ch, 1);
    return c & 0xFF;
}

void perror_soloader(const char *s) {
    l_warn("perror: %s: %s", s ? s : "", strerror(errno));
}

/*
 * Misc
 */

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
} utsname_bionic;

int uname_soloader(void *buf) {
    utsname_bionic *u = (utsname_bionic *)buf;
    memset(u, 0, sizeof(*u));
    strcpy(u->sysname, "Linux");
    strcpy(u->nodename, "psvita");
    strcpy(u->release, "2.6.35");
    strcpy(u->version, "#1 PREEMPT");
    strcpy(u->machine, "armv7l");
    strcpy(u->domainname, "localdomain");
    return 0;
}

// bionic/BSD values, which differ from newlib's
#define BIONIC_FP_INFINITE  0x01
#define BIONIC_FP_NAN       0x02
#define BIONIC_FP_NORMAL    0x04
#define BIONIC_FP_SUBNORMAL 0x08
#define BIONIC_FP_ZERO      0x10

int __fpclassifyd_soloader(double d) {
    switch (fpclassify(d)) {
        case FP_INFINITE:  return BIONIC_FP_INFINITE;
        case FP_NAN:       return BIONIC_FP_NAN;
        case FP_SUBNORMAL: return BIONIC_FP_SUBNORMAL;
        case FP_ZERO:      return BIONIC_FP_ZERO;
        default:           return BIONIC_FP_NORMAL;
    }
}

int __signbit_soloader(double d) {
    return signbit(d) ? 1 : 0;
}

uint32_t arc4random_soloader(void) {
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

/*
 * bionic's mbstate_t is smaller than newlib's, so never let newlib write into
 * the game's one. The game only feeds complete UTF-8 sequences in practice.
 */
size_t mbrtowc_soloader(wchar_t *pwc, const char *s, size_t n, void *ps) {
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    return mbrtowc(pwc, s, n, &st);
}

size_t wcrtomb_soloader(char *s, wchar_t wc, void *ps) {
    mbstate_t st;
    memset(&st, 0, sizeof(st));
    return wcrtomb(s, wc, &st);
}

char *setlocale_soloader(int category, const char *locale) {
    static char c_locale[] = "C";
    return c_locale;
}

int gethostname_soloader(char *name, size_t len) {
    strncpy(name, "psvita", len);
    if (len) name[len - 1] = '\0';
    return 0;
}

int select_soloader(int nfds, void *r, void *w, void *e, struct timeval *timeout) {
    // Networking is disabled, so select() is only useful as a sleep.
    if (timeout)
        sceKernelDelayThread(timeout->tv_sec * 1000000 + timeout->tv_usec);
    return 0;
}

void *getpwuid_soloader(uid_t uid) {
    return NULL;
}

void *getgrgid_soloader(gid_t gid) {
    return NULL;
}

uid_t getuid_soloader(void) {
    return 10000;
}

pid_t getpid_soloader(void) {
    return 1337;
}

/*
 * Networking: the game only uses it for ads/analytics/news, none of which
 * exist anymore. Fail fast instead of risking long timeouts.
 */

int socket_soloader(int domain, int type, int protocol) {
    errno = EAFNOSUPPORT;
    return -1;
}

int getaddrinfo_soloader(const char *node, const char *service, const void *hints, void **res) {
    if (res) *res = NULL;
    return 4; // EAI_FAIL (bionic)
}

void *gethostbyname_soloader(const char *name) {
    return NULL;
}

const char *gai_strerror_soloader(int err) {
    return "Network unavailable";
}
