/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      fgsfds
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  dialog.h
 * @brief Common dialog for error messages and cheats input.
 */

#ifndef SOLOADER_DIALOG_H
#define SOLOADER_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

int init_ime_dialog(const char *title, const char *initial_text);

char *get_ime_dialog_result(void);

int init_msg_dialog(const char *msg);

int get_msg_dialog_result(void);

/**
 * Start a non-blocking system message box with up to 3 buttons.
 * Keep calling gl_swap(GL_TRUE) and msgbox_poll() until it returns >= 0.
 */
int msgbox_start(const char *title, const char *msg,
                 const char **buttons, int nbuttons);

/**
 * @return -1 while the message box is open, otherwise the 0-based index of
 *         the pressed button (in the order passed to msgbox_start).
 */
int msgbox_poll(void);

void fatal_error(const char *fmt, ...) __attribute__((noreturn));

void warning(const char *msg);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_DIALOG_H
