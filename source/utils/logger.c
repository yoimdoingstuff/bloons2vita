/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"
#include "utils/paths.h"

#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <stdbool.h>
#include <stdatomic.h>

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);

static char message[2048];
static char line[2304];

static SceKernelLwMutexWork _file_mutex;
static atomic_bool _file_mutex_ready = ATOMIC_VAR_INIT(false);
static SceUID log_fd = -1;
static bool log_failed = false;
static char file_line[2304];

void log_file_write(const char *text) {
    if (log_failed || !text) return;

    if (!atomic_load_explicit(&_file_mutex_ready, memory_order_relaxed)) {
        if (sceKernelCreateLwMutex(&_file_mutex, "logfile_lock", 0, 0, NULL) < 0)
            return;
        atomic_store_explicit(&_file_mutex_ready, true, memory_order_relaxed);
    }
    sceKernelLockLwMutex(&_file_mutex, 1, NULL);

    if (log_fd < 0) {
        if (!data_path[0]) {
            // Called before paths_init(): don't create a stray log file
            sceKernelUnlockLwMutex(&_file_mutex, 1);
            return;
        }
        char path[96];
        sceClibSnprintf(path, sizeof(path), "%slog.txt", data_path);
        log_fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (log_fd < 0) {
            log_failed = true;
            sceKernelUnlockLwMutex(&_file_mutex, 1);
            return;
        }
    }

    // timestamp + text without ANSI escape sequences
    SceUInt32 us = sceKernelGetProcessTimeLow();
    int n = sceClibSnprintf(file_line, sizeof(file_line), "[%4u.%03u] ",
                            (unsigned)(us / 1000000), (unsigned)((us / 1000) % 1000));
    if (n < 0) n = 0;
    for (const char *p = text; *p && n < (int)sizeof(file_line) - 2; p++) {
        if (*p == '\x1B') {
            while (*p && *p != 'm') p++;
            if (!*p) break;
            continue;
        }
        file_line[n++] = *p;
    }
    if (n == 0 || file_line[n - 1] != '\n')
        file_line[n++] = '\n';

    // Unbuffered: the line is on the card even if we crash right after
    sceIoWrite(log_fd, file_line, n);

    sceKernelUnlockLwMutex(&_file_mutex, 1);
}

void _log_print(int t, const char* fmt, ...) {
    if (!atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        int ret = sceKernelCreateLwMutex(&_log_mutex, "log_lock", 0, 0, NULL);
        if (ret < 0) {
            sceClibPrintf("Error: failed to create log mutex: 0x%x\n", ret);
            return;
        }
        atomic_store_explicit(&_log_mutex_ready, true, memory_order_relaxed);
    }

    const char *color, *label;
    switch (t) {
        case LT_DEBUG:   color = COLOR_PINK;   label = "• debug  "; break;
        case LT_INFO:    color = COLOR_BLUE;   label = "ℹ info   "; break;
        case LT_WARN:    color = COLOR_ORANGE; label = "⚠ warning"; break;
        case LT_ERROR:   color = COLOR_RED;    label = "⨯ error  "; break;
        case LT_FATAL:   color = COLOR_RED;    label = "! fatal  "; break;
        case LT_SUCCESS: color = COLOR_GREEN;  label = "! success"; break;
        case LT_WAIT:    color = COLOR_CYAN;   label = "… waiting"; break;
        default: return;
    }

    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    va_list list;
    va_start(list, fmt);
    sceClibVsnprintf(message, sizeof(message), fmt, list);
    va_end(list);

    sceClibSnprintf(line, sizeof(line), " %s%s%s  %s\n", color, label, COLOR_END, message);
    sceClibPrintf("%s", line);
    log_file_write(line);

    sceKernelUnlockLwMutex(&_log_mutex, 1);
}
