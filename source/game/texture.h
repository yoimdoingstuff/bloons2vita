/*
 * texture.h
 *
 * Re-implementation of com.miniclip.nativeJNI.cocotexture: decodes an image
 * asset and hands the pixels back to the game via CocoJNI.MnativeInitBitmap.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_TEXTURE_H
#define BLOONS2_TEXTURE_H

#ifdef __cplusplus
extern "C" {
#endif

void texture_load(const char *path);

#ifdef __cplusplus
};
#endif

#endif // BLOONS2_TEXTURE_H
