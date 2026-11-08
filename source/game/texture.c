/*
 * texture.c
 *
 * Re-implementation of com.miniclip.nativeJNI.cocotexture: decodes an image
 * asset and hands the pixels back to the game via CocoJNI.MnativeInitBitmap.
 *
 * On Android the game asks Java to load a texture (cocojava.loadTexture),
 * Java decodes it with BitmapFactory into an ARGB_8888 Bitmap, copies the
 * pixels out with Bitmap.copyPixelsToBuffer() (RGBA byte order, alpha
 * premultiplied) and calls back into native code with them. We do the same
 * with stb_image.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/texture.h"

#include <malloc.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include <stb/stb_image.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>

#include "game/apk.h"
#include "game/coco.h"
#include "utils/logger.h"

// The game may keep using the array it got from MnativeInitBitmap until the
// texture is uploaded after loadTexture() returns, so the previous bitmap is
// only released when the next one is requested.
static jbyteArray last_bitmap = NULL;

static void premultiply(uint8_t *px, size_t count) {
    for (size_t i = 0; i < count; i++, px += 4) {
        uint32_t a = px[3];
        if (a == 255) continue;
        px[0] = (uint8_t)((px[0] * a + 127) / 255);
        px[1] = (uint8_t)((px[1] * a + 127) / 255);
        px[2] = (uint8_t)((px[2] * a + 127) / 255);
    }
}

void texture_load(const char *path) {
    if (last_bitmap) {
        jda_free((JavaDynArray *)last_bitmap);
        last_bitmap = NULL;
    }

    uint8_t *file;
    size_t size;
    if (!path || !asset_load(path, &file, &size)) {
        l_error("texture_load(\"%s\"): asset not found", path ? path : "(null)");
        coco.MnativeInitBitmap(&jni, COCO_CLASS, 0, 0, NULL);
        return;
    }

    int w, h, comp;
    uint8_t *px = stbi_load_from_memory(file, (int)size, &w, &h, &comp, 4);
    free(file);

    if (!px) {
        l_error("texture_load(\"%s\"): decode failed: %s", path, stbi_failure_reason());
        coco.MnativeInitBitmap(&jni, COCO_CLASS, 0, 0, NULL);
        return;
    }

    size_t bytes = (size_t)w * (size_t)h * 4;
    premultiply(px, (size_t)w * (size_t)h);

    jbyteArray arr = jni->NewByteArray(&jni, (jsize)bytes);
    if (!arr) {
        stbi_image_free(px);
        l_error("texture_load(\"%s\"): out of memory (%ix%i)", path, w, h);
        coco.MnativeInitBitmap(&jni, COCO_CLASS, 0, 0, NULL);
        return;
    }

    memcpy(((JavaDynArray *)arr)->array, px, bytes);
    stbi_image_free(px);

    l_debug("texture_load(\"%s\"): %ix%i", path, w, h);

    last_bitmap = arr;
    coco.MnativeInitBitmap(&jni, COCO_CLASS, w, h, arr);
}
