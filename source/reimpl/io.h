/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  io.h
 * @brief Wrappers and implementations for some of the IO functions.
 */

#ifndef SOLOADER_IO_H
#define SOLOADER_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>
#include <sys/fcntl.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef DT_DIR
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
#endif

/*
 * bionic's `struct stat` on 32-bit ARM (104 bytes). Written with explicit
 * fixed-width fields and padding: newlib's nlink_t/uid_t/gid_t are 16-bit
 * and its timespec differs, and the 64-bit fields are 8-byte aligned in
 * bionic (st_size lives at offset 48). Getting this wrong makes fstat()
 * report garbage sizes, which broke the game's own zip (apk) reader.
 */
typedef struct __attribute__((__packed__)) stat64_bionic {
    uint64_t st_dev;          // 0
    uint8_t  __pad0[4];       // 8
    uint32_t __st_ino;        // 12
    uint32_t st_mode;         // 16
    uint32_t st_nlink;        // 20
    uint32_t st_uid;          // 24
    uint32_t st_gid;          // 28
    uint64_t st_rdev;         // 32
    uint8_t  __pad3[4];       // 40
    uint8_t  __align0[4];     // 44: alignment of st_size
    int64_t  st_size;         // 48
    uint32_t st_blksize;      // 56
    uint8_t  __align1[4];     // 60: alignment of st_blocks
    uint64_t st_blocks;       // 64
    uint32_t atime_sec;       // 72
    uint32_t atime_nsec;      // 76
    uint32_t mtime_sec;       // 80
    uint32_t mtime_nsec;      // 84
    uint32_t ctime_sec;       // 88
    uint32_t ctime_nsec;      // 92
    uint64_t st_ino;          // 96
} stat64_bionic;

_Static_assert(sizeof(stat64_bionic) == 104, "bionic struct stat is 104 bytes");
_Static_assert(__builtin_offsetof(stat64_bionic, st_mode) == 16, "st_mode offset");
_Static_assert(__builtin_offsetof(stat64_bionic, st_size) == 48, "st_size offset");
_Static_assert(__builtin_offsetof(stat64_bionic, st_ino) == 96, "st_ino offset");

typedef struct __attribute__((__packed__)) dirent64_bionic {
    int16_t d_ino; // 2 bytes // offset 0x0
    int64_t d_off; // 8 bytes // offset 0x2
    uint64_t d_reclen; // 8 bytes // 0xA
    unsigned char d_type; // 1 byte // offset 0x12
    char d_name[256]; // 256 bytes // offset 0x13
} dirent64_bionic;

/*
 * The game is given the same paths it would get on Android; remap_path()
 * translates them (and anything else Android-specific it may hardcode) to
 * locations in the data folder (ux0:data/bloons2/ or ur0:data/bloons2/,
 * see utils/paths.h):
 *
 *   /data/app/com.miniclip.bloons2-1.apk           -> <data>game.apk
 *   /data/data/com.miniclip.bloons2/files/...      -> <data>files/...
 *   /sdcard/Android/data/com.miniclip.bloons2/...  -> <data>external/...
 *   /data/data/com.miniclip.bloons2/...            -> <data>...
 *   /sdcard/... , /mnt/sdcard/...                  -> <data>...
 */
#define ANDROID_APK_PATH      "/data/app/com.miniclip.bloons2-1.apk"
#define ANDROID_PKG_DATA_PATH "/data/data/com.miniclip.bloons2"
#define ANDROID_FILES_PATH    ANDROID_PKG_DATA_PATH "/files"
#define ANDROID_EXTERNAL_PATH "/sdcard/Android/data/com.miniclip.bloons2"

/**
 * Map an Android path to its Vita equivalent. Returns either `path` itself
 * or `out`.
 */
const char *remap_path(const char *path, char *out, size_t out_size);

int open_soloader(const char * path, int oflag, ...);

int lstat_soloader(const char * path, stat64_bionic * buf);

int access_soloader(const char *path, int mode);

int mkdir_soloader(const char *path, mode_t mode);

int rmdir_soloader(const char *path);

int remove_soloader(const char *path);

int rename_soloader(const char *old_path, const char *new_path);

int chdir_soloader(const char *path);

char *getcwd_soloader(char *buf, size_t size);

off_t lseek_soloader(int fd, off_t offset, int whence);

int symlink_soloader(const char *target, const char *linkpath);

ssize_t readlink_soloader(const char *path, char *buf, size_t bufsiz);

void *gzopen_soloader(const char *path, const char *mode);

FILE * fopen_soloader(const char * filename, const char * mode);

DIR *opendir_soloader(char *name);

int stat_soloader(const char * path, stat64_bionic * buf);

int fstat_soloader(int fd, stat64_bionic * buf);

struct dirent64_bionic * readdir_soloader(DIR *dir);

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result);

int close_soloader(int fd);

int fclose_soloader(FILE *f);

int closedir_soloader(DIR *dir);

int fcntl_soloader(int fd, int cmd, ...);

int ioctl_soloader(int fd, int request, ... /* arg */);

int fsync_soloader(int fd);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_IO_H
