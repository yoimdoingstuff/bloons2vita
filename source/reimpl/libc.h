/*
 * libc.h
 *
 * Bionic-compatible replacements for libc bits that differ from newlib:
 * the __sF standard streams, fpclassify constants, utsname, mbstate_t, etc.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef SOLOADER_LIBC_H
#define SOLOADER_LIBC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

// bionic's `FILE __sF[3]` (stdin, stdout, stderr); sizeof(FILE) is 84 there
#define BIONIC_FILE_SIZE 84
extern uint8_t __sF_fake[3][BIONIC_FILE_SIZE];

extern char **environ_fake;

// stdio functions that may receive one of the fake std streams
int fclose_sf(FILE *f);
int fflush_sf(FILE *f);
int fgetc_sf(FILE *f);
char *fgets_sf(char *s, int n, FILE *f);
int fprintf_sf(FILE *f, const char *fmt, ...);
int vfprintf_sf(FILE *f, const char *fmt, va_list ap);
int fputc_sf(int c, FILE *f);
int fputs_sf(const char *s, FILE *f);
size_t fread_sf(void *ptr, size_t size, size_t n, FILE *f);
size_t fwrite_sf(const void *ptr, size_t size, size_t n, FILE *f);
int fseek_sf(FILE *f, long off, int whence);
long ftell_sf(FILE *f);
void rewind_sf(FILE *f);
int setvbuf_sf(FILE *f, char *buf, int mode, size_t size);
int ungetc_sf(int c, FILE *f);
wint_t getwc_sf(FILE *f);
wint_t putwc_sf(wchar_t c, FILE *f);
wint_t ungetwc_sf(wint_t c, FILE *f);

// console output
int printf_soloader(const char *fmt, ...);
int vprintf_soloader(const char *fmt, va_list ap);
int puts_soloader(const char *s);
int putchar_soloader(int c);
void perror_soloader(const char *s);

// misc
int uname_soloader(void *buf);
int __fpclassifyd_soloader(double d);
int __signbit_soloader(double d);
uint32_t arc4random_soloader(void);
size_t mbrtowc_soloader(wchar_t *pwc, const char *s, size_t n, void *ps);
size_t wcrtomb_soloader(char *s, wchar_t wc, void *ps);
char *setlocale_soloader(int category, const char *locale);
int gethostname_soloader(char *name, size_t len);
int select_soloader(int nfds, void *r, void *w, void *e, struct timeval *timeout);
void *getpwuid_soloader(uid_t uid);
void *getgrgid_soloader(gid_t gid);
uid_t getuid_soloader(void);
pid_t getpid_soloader(void);
int socket_soloader(int domain, int type, int protocol);
int getaddrinfo_soloader(const char *node, const char *service, const void *hints, void **res);
void *gethostbyname_soloader(const char *name);
const char *gai_strerror_soloader(int err);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_LIBC_H
