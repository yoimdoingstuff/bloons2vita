/*
 * patch.h
 *
 * Offsets specific to the supported build of libbloons2.so
 * (Bloons 2 for Android v1.0.0, lib/armeabi/libbloons2.so,
 * SHA1 4E10AE9EB511C647FC02A4D4AC923E3D01F67882).
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#ifndef BLOONS2_PATCH_H
#define BLOONS2_PATCH_H

#define SO_SHA1 "4E10AE9EB511C647FC02A4D4AC923E3D01F67882"

// PT_ARM_EXIDX segment, relative to the start of the text segment
#define SO_EXIDX_OFFSET 0x006D3C30
#define SO_EXIDX_SIZE   0x000044A8

void so_patch(void);

#endif // BLOONS2_PATCH_H
