/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  dynlib.c
 * @brief Resolving dynamic imports of libbloons2.so.
 *
 * Contains exactly the symbols the game imports (from bionic libc, libm,
 * libz, libdl, liblog, libGLESv1_CM and libcrypto's MD5), mapped to
 * Vita/newlib implementations or bionic-compatible wrappers.
 */

#include <psp2/kernel/clib.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>
#include <sys/time.h>

#include <openssl/md5.h>
#include <vitaGL.h>

#include <so_util/so_util.h>

#include "utils/logger.h"
#include "utils/utils.h"

#include "reimpl/errno.h"
#include "reimpl/io.h"
#include "reimpl/libc.h"
#include "reimpl/log.h"
#include "reimpl/mem.h"
#include "reimpl/pthr.h"
#include "reimpl/sys.h"

#include "patch.h"

#include "pthread-svelte/include/pthread_svelte.h"

extern so_module so_mod;

extern void *__cxa_atexit;
extern void *__cxa_finalize;

extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;

static const unsigned int __page_size_fake = PAGE_SIZE;
static uintptr_t __stack_chk_guard_fake = 0x00000aff;

extern so_default_dynlib default_dynlib[];
extern const size_t default_dynlib_count;

/*
 * libdl: the game may dlsym() things it expects to find in system
 * libraries, so look them up in our own import table first.
 */
static void *dlsym_soloader(void *handle, const char *symbol) {
    for (size_t i = 0; i < default_dynlib_count; i++) {
        if (strcmp(default_dynlib[i].symbol, symbol) == 0)
            return (void *)default_dynlib[i].func;
    }

    uintptr_t own = so_symbol(&so_mod, symbol);
    if (own)
        return (void *)own;

    l_error("dlsym: Unknown symbol \"%s\".", symbol);
    return NULL;
}

/*
 * C++ exception unwinding: the game's statically linked unwinder asks the
 * dynamic linker for the .ARM.exidx table that covers a given PC.
 */
static void *__gnu_Unwind_Find_exidx_soloader(uintptr_t pc, int *pcount) {
    if (pc >= so_mod.text_base && pc < so_mod.text_base + so_mod.text_size) {
        *pcount = (int)(SO_EXIDX_SIZE / 8);
        return (void *)(so_mod.text_base + SO_EXIDX_OFFSET);
    }
    *pcount = 0;
    return NULL;
}

/*
 * Networking stubs (see reimpl/libc.c)
 */
static char *inet_ntoa_soloader(uint32_t in) {
    static char buf[16];
    const uint8_t *b = (const uint8_t *)&in;
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}

static const char *inet_ntop_soloader(int af, const void *src, char *dst, unsigned int size) {
    if (af != 2 /* AF_INET */ || size < 16) return NULL;
    const uint8_t *b = (const uint8_t *)src;
    snprintf(dst, size, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return dst;
}

/*
 * GL
 */
static void glPointSizePointerOES_soloader(GLenum type, GLsizei stride, const void *ptr) {
    // Not supported by vitaGL; the game only uses it for point sprites.
}

so_default_dynlib default_dynlib[] = {
        // C/C++ runtime internals
        { "__aeabi_memcpy", (uintptr_t)&sceClibMemcpy },
        { "__aeabi_memmove", (uintptr_t)&memmove },
        { "__cxa_atexit", (uintptr_t)&__cxa_atexit },
        { "__cxa_finalize", (uintptr_t)&__cxa_finalize },
        { "__gnu_Unwind_Find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx_soloader },
        { "__page_size", (uintptr_t)&__page_size_fake },
        { "__stack_chk_fail", (uintptr_t)&__stack_chk_fail_soloader },
        { "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },
        { "abort", (uintptr_t)&abort_soloader },
        { "exit", (uintptr_t)&exit_soloader },
        { "environ", (uintptr_t)&environ_fake },


        // ctype / wctype
        { "_ctype_", (uintptr_t)&BIONIC_ctype_ },
        { "_tolower_tab_", (uintptr_t)&BIONIC_tolower_tab_ },
        { "isalnum", (uintptr_t)&isalnum },
        { "isalpha", (uintptr_t)&isalpha },
        { "isdigit", (uintptr_t)&isdigit },
        { "isspace", (uintptr_t)&isspace },
        { "tolower", (uintptr_t)&tolower },
        { "toupper", (uintptr_t)&toupper },
        { "iswctype", (uintptr_t)&iswctype },
        { "towlower", (uintptr_t)&towlower },
        { "towupper", (uintptr_t)&towupper },
        { "wctype", (uintptr_t)&wctype },


        // Android logging
        { "__android_log_print", (uintptr_t)&__android_log_print },
        { "__android_log_vprint", (uintptr_t)&__android_log_vprint },


        // Math
        { "__fpclassifyd", (uintptr_t)&__fpclassifyd_soloader },
        { "__signbit", (uintptr_t)&__signbit_soloader },
        { "acos", (uintptr_t)&acos },
        { "asin", (uintptr_t)&asin },
        { "atan", (uintptr_t)&atan },
        { "atan2", (uintptr_t)&atan2 },
        { "ceil", (uintptr_t)&ceil },
        { "copysign", (uintptr_t)&copysign },
        { "cos", (uintptr_t)&cos },
        { "cosh", (uintptr_t)&cosh },
        { "exp", (uintptr_t)&exp },
        { "fabs", (uintptr_t)&fabs },
        { "floor", (uintptr_t)&floor },
        { "fmax", (uintptr_t)&fmax },
        { "fmaxf", (uintptr_t)&fmaxf },
        { "fmod", (uintptr_t)&fmod },
        { "frexp", (uintptr_t)&frexp },
        { "ldexp", (uintptr_t)&ldexp },
        { "log", (uintptr_t)&log },
        { "log10", (uintptr_t)&log10 },
        { "modf", (uintptr_t)&modf },
        { "pow", (uintptr_t)&pow },
        { "roundf", (uintptr_t)&roundf },
        { "sin", (uintptr_t)&sin },
        { "sinh", (uintptr_t)&sinh },
        { "sqrt", (uintptr_t)&sqrt },
        { "tan", (uintptr_t)&tan },
        { "tanh", (uintptr_t)&tanh },


        // Networking (disabled, see reimpl/libc.c)
        { "accept", (uintptr_t)&retminus1 },
        { "bind", (uintptr_t)&retminus1 },
        { "connect", (uintptr_t)&retminus1 },
        { "freeaddrinfo", (uintptr_t)&ret0 },
        { "gai_strerror", (uintptr_t)&gai_strerror_soloader },
        { "getaddrinfo", (uintptr_t)&getaddrinfo_soloader },
        { "gethostbyaddr", (uintptr_t)&ret0 },
        { "gethostbyname", (uintptr_t)&gethostbyname_soloader },
        { "gethostname", (uintptr_t)&gethostname_soloader },
        { "getpeername", (uintptr_t)&retminus1 },
        { "getsockname", (uintptr_t)&retminus1 },
        { "getsockopt", (uintptr_t)&retminus1 },
        { "inet_addr", (uintptr_t)&retminus1 }, // INADDR_NONE
        { "inet_ntoa", (uintptr_t)&inet_ntoa_soloader },
        { "inet_ntop", (uintptr_t)&inet_ntop_soloader },
        { "inet_pton", (uintptr_t)&ret0 },
        { "listen", (uintptr_t)&retminus1 },
        { "recv", (uintptr_t)&retminus1 },
        { "recvfrom", (uintptr_t)&retminus1 },
        { "select", (uintptr_t)&select_soloader },
        { "send", (uintptr_t)&retminus1 },
        { "sendto", (uintptr_t)&retminus1 },
        { "setsockopt", (uintptr_t)&retminus1 },
        { "socket", (uintptr_t)&socket_soloader },


        // Memory
        { "bzero", (uintptr_t)&bzero },
        { "calloc", (uintptr_t)&calloc },
        { "free", (uintptr_t)&free },
        { "malloc", (uintptr_t)&malloc },
        { "memchr", (uintptr_t)&memchr },
        { "memcmp", (uintptr_t)&memcmp },
        { "memcpy", (uintptr_t)&sceClibMemcpy },
        { "memmove", (uintptr_t)&memmove },
        { "memset", (uintptr_t)&memset },
        { "mmap", (uintptr_t)&mmap },
        { "munmap", (uintptr_t)&munmap },
        { "realloc", (uintptr_t)&realloc },


        // File system / fd IO
        { "access", (uintptr_t)&access_soloader },
        { "basename", (uintptr_t)&basename },
        { "chdir", (uintptr_t)&chdir_soloader },
        { "close", (uintptr_t)&close_soloader },
        { "closedir", (uintptr_t)&closedir_soloader },
        { "dup2", (uintptr_t)&retminus1 },
        { "fcntl", (uintptr_t)&fcntl_soloader },
        { "fstat", (uintptr_t)&fstat_soloader },
        { "fsync", (uintptr_t)&fsync_soloader },
        { "ftruncate", (uintptr_t)&ftruncate },
        { "getcwd", (uintptr_t)&getcwd },
        { "ioctl", (uintptr_t)&ioctl_soloader },
        { "lseek", (uintptr_t)&lseek },
        { "lstat", (uintptr_t)&lstat_soloader },
        { "mkdir", (uintptr_t)&mkdir_soloader },
        { "open", (uintptr_t)&open_soloader },
        { "opendir", (uintptr_t)&opendir_soloader },
        { "pipe", (uintptr_t)&retminus1 },
        { "read", (uintptr_t)&read },
        { "readdir", (uintptr_t)&readdir_soloader },
        { "readlink", (uintptr_t)&readlink_soloader },
        { "remove", (uintptr_t)&remove_soloader },
        { "rename", (uintptr_t)&rename_soloader },
        { "rmdir", (uintptr_t)&rmdir_soloader },
        { "stat", (uintptr_t)&stat_soloader },
        { "symlink", (uintptr_t)&symlink_soloader },
        { "write", (uintptr_t)&write },


        // stdio
        { "__sF", (uintptr_t)&__sF_fake },
        { "fclose", (uintptr_t)&fclose_sf },
        { "fdopen", (uintptr_t)&fdopen },
        { "fflush", (uintptr_t)&fflush_sf },
        { "fgetc", (uintptr_t)&fgetc_sf },
        { "fgets", (uintptr_t)&fgets_sf },
        { "fopen", (uintptr_t)&fopen_soloader },
        { "fprintf", (uintptr_t)&fprintf_sf },
        { "fputc", (uintptr_t)&fputc_sf },
        { "fputs", (uintptr_t)&fputs_sf },
        { "fread", (uintptr_t)&fread_sf },
        { "fseek", (uintptr_t)&fseek_sf },
        { "ftell", (uintptr_t)&ftell_sf },
        { "fwrite", (uintptr_t)&fwrite_sf },
        { "getc", (uintptr_t)&fgetc_sf },
        { "getwc", (uintptr_t)&getwc_sf },
        { "perror", (uintptr_t)&perror_soloader },
        { "printf", (uintptr_t)&printf_soloader },
        { "putc", (uintptr_t)&fputc_sf },
        { "putchar", (uintptr_t)&putchar_soloader },
        { "puts", (uintptr_t)&puts_soloader },
        { "putwc", (uintptr_t)&putwc_sf },
        { "rewind", (uintptr_t)&rewind_sf },
        { "setvbuf", (uintptr_t)&setvbuf_sf },
        { "snprintf", (uintptr_t)&snprintf },
        { "sprintf", (uintptr_t)&sprintf },
        { "sscanf", (uintptr_t)&sscanf },
        { "ungetc", (uintptr_t)&ungetc_sf },
        { "ungetwc", (uintptr_t)&ungetwc_sf },
        { "vfprintf", (uintptr_t)&vfprintf_sf },
        { "vprintf", (uintptr_t)&vprintf_soloader },
        { "vsnprintf", (uintptr_t)&vsnprintf },
        { "vsprintf", (uintptr_t)&vsprintf },
        { "vswprintf", (uintptr_t)&vswprintf },


        // OpenGL ES 1.x
        { "glAlphaFunc", (uintptr_t)&glAlphaFunc },
        { "glBindTexture", (uintptr_t)&glBindTexture },
        { "glBlendFunc", (uintptr_t)&glBlendFunc },
        { "glClear", (uintptr_t)&glClear },
        { "glClearColor", (uintptr_t)&glClearColor },
        { "glClearDepthf", (uintptr_t)&glClearDepthf },
        { "glColor4f", (uintptr_t)&glColor4f },
        { "glColor4ub", (uintptr_t)&glColor4ub },
        { "glColorPointer", (uintptr_t)&glColorPointer },
        { "glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D },
        { "glDeleteTextures", (uintptr_t)&glDeleteTextures },
        { "glDepthFunc", (uintptr_t)&glDepthFunc },
        { "glDisable", (uintptr_t)&glDisable },
        { "glDisableClientState", (uintptr_t)&glDisableClientState },
        { "glDrawArrays", (uintptr_t)&glDrawArrays },
        { "glDrawElements", (uintptr_t)&glDrawElements },
        { "glEnable", (uintptr_t)&glEnable },
        { "glEnableClientState", (uintptr_t)&glEnableClientState },
        { "glFrustumf", (uintptr_t)&glFrustumf },
        { "glGenTextures", (uintptr_t)&glGenTextures },
        { "glGetError", (uintptr_t)&glGetError },
        { "glGetFloatv", (uintptr_t)&glGetFloatv },
        { "glGetIntegerv", (uintptr_t)&glGetIntegerv },
        { "glGetString", (uintptr_t)&glGetString },
        { "glLineWidth", (uintptr_t)&glLineWidth },
        { "glLoadIdentity", (uintptr_t)&glLoadIdentity },
        { "glMatrixMode", (uintptr_t)&glMatrixMode },
        { "glMultMatrixf", (uintptr_t)&glMultMatrixf },
        { "glOrthof", (uintptr_t)&glOrthof },
        { "glPixelStorei", (uintptr_t)&glPixelStorei },
        { "glPointSize", (uintptr_t)&glPointSize },
        { "glPointSizePointerOES", (uintptr_t)&glPointSizePointerOES_soloader },
        { "glPopMatrix", (uintptr_t)&glPopMatrix },
        { "glPushMatrix", (uintptr_t)&glPushMatrix },
        { "glReadPixels", (uintptr_t)&glReadPixels },
        { "glRotatef", (uintptr_t)&glRotatef },
        { "glScalef", (uintptr_t)&glScalef },
        { "glScissor", (uintptr_t)&glScissor },
        { "glTexCoordPointer", (uintptr_t)&glTexCoordPointer },
        { "glTexEnvi", (uintptr_t)&glTexEnvi },
        { "glTexImage2D", (uintptr_t)&glTexImage2D },
        { "glTexParameteri", (uintptr_t)&glTexParameteri },
        { "glTranslatef", (uintptr_t)&glTranslatef },
        { "glVertexPointer", (uintptr_t)&glVertexPointer },
        { "glViewport", (uintptr_t)&glViewport },


        // Pthread
        { "pthread_cond_broadcast", (uintptr_t)&pthread_svelte_cond_broadcast },
        { "pthread_cond_destroy", (uintptr_t)&pthread_svelte_cond_destroy },
        { "pthread_cond_init", (uintptr_t)&pthread_svelte_cond_init },
        { "pthread_cond_signal", (uintptr_t)&pthread_svelte_cond_signal },
        { "pthread_cond_timedwait", (uintptr_t)&pthread_svelte_cond_timedwait },
        { "pthread_cond_wait", (uintptr_t)&pthread_svelte_cond_wait },
        { "pthread_create", (uintptr_t)&pthread_create_soloader },
        { "pthread_getschedparam", (uintptr_t)&pthread_getschedparam_soloader },
        { "pthread_getspecific", (uintptr_t)&pthread_getspecific },
        { "pthread_key_create", (uintptr_t)&pthread_key_create },
        { "pthread_key_delete", (uintptr_t)&pthread_key_delete },
        { "pthread_mutex_destroy", (uintptr_t)&pthread_svelte_mutex_destroy },
        { "pthread_mutex_init", (uintptr_t)&pthread_svelte_mutex_init },
        { "pthread_mutex_lock", (uintptr_t)&pthread_svelte_mutex_lock },
        { "pthread_mutex_trylock", (uintptr_t)&pthread_svelte_mutex_trylock },
        { "pthread_mutex_unlock", (uintptr_t)&pthread_svelte_mutex_unlock },
        { "pthread_once", (uintptr_t)&pthread_svelte_once },
        { "pthread_self", (uintptr_t)&pthread_self_soloader },
        { "pthread_setschedparam", (uintptr_t)&pthread_setschedparam_soloader },
        { "pthread_setspecific", (uintptr_t)&pthread_setspecific },
        { "sched_get_priority_min", (uintptr_t)&sched_get_priority_min },


        // wchar
        { "btowc", (uintptr_t)&btowc },
        { "mbrtowc", (uintptr_t)&mbrtowc_soloader },
        { "wcrtomb", (uintptr_t)&wcrtomb_soloader },
        { "wcscoll", (uintptr_t)&wcscoll },
        { "wcsftime", (uintptr_t)&wcsftime },
        { "wcslen", (uintptr_t)&wcslen },
        { "wcsxfrm", (uintptr_t)&wcsxfrm },
        { "wctob", (uintptr_t)&wctob },
        { "wmemchr", (uintptr_t)&wmemchr },
        { "wmemcmp", (uintptr_t)&wmemcmp },
        { "wmemcpy", (uintptr_t)&wmemcpy },
        { "wmemmove", (uintptr_t)&wmemmove },
        { "wmemset", (uintptr_t)&wmemset },


        // libdl
        { "dlclose", (uintptr_t)&ret0 },
        { "dlerror", (uintptr_t)&ret0 },
        { "dlopen", (uintptr_t)&ret1 },
        { "dlsym", (uintptr_t)&dlsym_soloader },


        // Errno
        { "__errno", (uintptr_t)&__errno_soloader },
        { "strerror", (uintptr_t)&strerror_soloader },
        { "strerror_r", (uintptr_t)&strerror_r_soloader },


        // Strings
        { "strcasecmp", (uintptr_t)&strcasecmp },
        { "strcat", (uintptr_t)&strcat },
        { "strchr", (uintptr_t)&strchr },
        { "strcmp", (uintptr_t)&strcmp },
        { "strcoll", (uintptr_t)&strcoll },
        { "strcpy", (uintptr_t)&strcpy },
        { "strdup", (uintptr_t)&strdup },
        { "strlen", (uintptr_t)&strlen },
        { "strncasecmp", (uintptr_t)&strncasecmp },
        { "strncat", (uintptr_t)&strncat },
        { "strncmp", (uintptr_t)&strncmp },
        { "strncpy", (uintptr_t)&strncpy },
        { "strrchr", (uintptr_t)&strrchr },
        { "strstr", (uintptr_t)&strstr },
        { "strtok", (uintptr_t)&strtok },
        { "strtok_r", (uintptr_t)&strtok_r },
        { "strxfrm", (uintptr_t)&strxfrm },


        // Processes / users / signals (no such thing on the Vita)
        { "__wait4", (uintptr_t)&retminus1 },
        { "alarm", (uintptr_t)&ret0 },
        { "bsd_signal", (uintptr_t)&signal },
        { "execve", (uintptr_t)&retminus1 },
        { "fork", (uintptr_t)&retminus1 },
        { "geteuid", (uintptr_t)&getuid_soloader },
        { "getgrgid", (uintptr_t)&getgrgid_soloader },
        { "getpid", (uintptr_t)&getpid_soloader },
        { "getpwuid", (uintptr_t)&getpwuid_soloader },
        { "getuid", (uintptr_t)&getuid_soloader },
        { "kill", (uintptr_t)&retminus1 },
        { "raise", (uintptr_t)&ret0 },
        { "sigaction", (uintptr_t)&sigaction },
        { "uname", (uintptr_t)&uname_soloader },


        // Time
        { "clock", (uintptr_t)&clock_soloader },
        { "ctime", (uintptr_t)&ctime },
        { "gettimeofday", (uintptr_t)&gettimeofday },
        { "gmtime", (uintptr_t)&gmtime },
        { "gmtime_r", (uintptr_t)&gmtime_r },
        { "mktime", (uintptr_t)&mktime },
        { "sleep", (uintptr_t)&sleep },
        { "strftime", (uintptr_t)&strftime },
        { "time", (uintptr_t)&time },
        { "usleep", (uintptr_t)&usleep },


        // stdlib
        { "arc4random", (uintptr_t)&arc4random_soloader },
        { "atoi", (uintptr_t)&atoi },
        { "bsearch", (uintptr_t)&bsearch },
        { "getenv", (uintptr_t)&getenv_soloader },
        { "lrand48", (uintptr_t)&lrand48 },
        { "qsort", (uintptr_t)&qsort },
        { "setlocale", (uintptr_t)&setlocale_soloader },
        { "srand48", (uintptr_t)&srand48 },
        { "strtod", (uintptr_t)&strtod },
        { "strtol", (uintptr_t)&strtol },
        { "strtoll", (uintptr_t)&strtoll },
        { "strtoul", (uintptr_t)&strtoul },


        // setjmp. bionic's jmp_buf (64 words) is larger than newlib's, so
        // newlib can safely use the game's buffers. sigsetjmp/siglongjmp
        // must point straight at setjmp/longjmp (no wrapper frame).
        { "setjmp", (uintptr_t)&setjmp },
        { "longjmp", (uintptr_t)&longjmp },
        { "sigsetjmp", (uintptr_t)&setjmp },
        { "siglongjmp", (uintptr_t)&longjmp },


        // zlib
        { "crc32", (uintptr_t)&crc32 },
        { "deflate", (uintptr_t)&deflate },
        { "deflateEnd", (uintptr_t)&deflateEnd },
        { "deflateInit2_", (uintptr_t)&deflateInit2_ },
        { "deflateInit_", (uintptr_t)&deflateInit_ },
        { "deflateReset", (uintptr_t)&deflateReset },
        { "gzclose", (uintptr_t)&gzclose },
        { "gzopen", (uintptr_t)&gzopen_soloader },
        { "gzread", (uintptr_t)&gzread },
        { "inflate", (uintptr_t)&inflate },
        { "inflateEnd", (uintptr_t)&inflateEnd },
        { "inflateInit2_", (uintptr_t)&inflateInit2_ },
        { "inflateInit_", (uintptr_t)&inflateInit_ },
        { "inflateReset", (uintptr_t)&inflateReset },
        { "uncompress", (uintptr_t)&uncompress },
        { "zError", (uintptr_t)&zError },
        { "zlibVersion", (uintptr_t)&zlibVersion },


        // libcrypto
        { "MD5_Final", (uintptr_t)&MD5_Final },
        { "MD5_Init", (uintptr_t)&MD5_Init },
        { "MD5_Update", (uintptr_t)&MD5_Update },
};

const size_t default_dynlib_count = sizeof(default_dynlib) / sizeof(default_dynlib[0]);

void resolve_imports(so_module* mod) {
    so_resolve(mod, default_dynlib, sizeof(default_dynlib), 0);
}
