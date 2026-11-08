/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  glutil.h
 * @brief OpenGL API initializer, related functions.
 */

#ifndef SOLOADER_GLUTIL_H
#define SOLOADER_GLUTIL_H

#include <vitaGL.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Set once gl_init() has initialized vitaGL. */
extern GLboolean gl_initialized;

void gl_init();

void gl_preload();

/**
 * Present the current frame.
 * @param has_commondialog Pass GL_TRUE while a system dialog (message box,
 *                         IME keyboard) is on screen so vitaGL draws it.
 */
void gl_swap(GLboolean has_commondialog);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_GLUTIL_H
