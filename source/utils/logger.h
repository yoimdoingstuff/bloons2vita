/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  logger.h
 * @brief Logging utilities.
 */

#ifndef SOLOADER_LOGGER_H
#define SOLOADER_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#define LT_DEBUG   0
#define LT_INFO    1
#define LT_WARN    2
#define LT_ERROR   3
#define LT_FATAL   4
#define LT_SUCCESS 5
#define LT_WAIT    6

// Everything is printed with sceClibPrintf and appended to log.txt in the
// data folder (recreated on every launch, written unbuffered so the last
// lines survive a crash). Release builds keep startup milestones, warnings
// and errors; Debug builds add info and debug detail.
#ifdef DEBUG_SOLOADER
#define l_debug(...)   _log_print(LT_DEBUG,   __VA_ARGS__)
#define l_info(...)    _log_print(LT_INFO,    __VA_ARGS__)
#else
#define l_debug(...)
#define l_info(...)
#endif

#define l_warn(...)    _log_print(LT_WARN,    __VA_ARGS__)
#define l_success(...) _log_print(LT_SUCCESS, __VA_ARGS__)
#define l_wait(...)    _log_print(LT_WAIT,    __VA_ARGS__)
#define l_error(...)   _log_print(LT_ERROR,   __VA_ARGS__)
#define l_fatal(...)   _log_print(LT_FATAL,   __VA_ARGS__)

void _log_print(int t, const char* fmt, ...)
                __attribute__ ((format (printf, 2, 3)));

/** Append a raw line to log.txt (ANSI color codes are stripped). */
void log_file_write(const char *line);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_LOGGER_H
