/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/glutil.h"

#include "utils/utils.h"
#include "utils/dialog.h"
#include "utils/logger.h"

void gl_preload() {
    // Bloons 2 is a pure OpenGL ES 1.x title: it never submits shaders of its
    // own, but vitaGL's fixed-function pipeline emulation still compiles its
    // internal shaders at runtime, which needs the shader compiler module.
    if (!file_exists("ur0:/data/libshacccg.suprx")
        && !file_exists("ur0:/data/external/libshacccg.suprx")) {
        fatal_error("Error: libshacccg.suprx is not installed. "
                    "Google \"ShaRKBR33D\" for quick installation.");
    }
}

GLboolean gl_initialized = GL_FALSE;

void gl_init() {
    // 2D sprite game: no MSAA needed. The threshold is the amount of memory
    // vitaGL leaves untouched for everything else (thread stacks, libzip...).
    vglInitExtended(0, 960, 544, 16 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    gl_initialized = GL_TRUE;
}

void gl_swap(GLboolean has_commondialog) {
    vglSwapBuffers(has_commondialog);
}
