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

typedef struct __attribute__((__packed__)) stat64_bionic {
    unsigned long long st_dev;
    unsigned char __pad0[4];
    unsigned long __st_ino;
    unsigned int st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    unsigned long long st_rdev;
    unsigned char __pad3[4];
    long long st_size;
    unsigned long st_blksize;
    unsigned long long st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    unsigned long long st_ino;
} stat64_bionic;

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
 * locations under DATA_PATH:
 *
 *   /data/app/com.miniclip.bloons2-1.apk           -> APK_PATH
 *   /data/data/com.miniclip.bloons2/files/...      -> DATA_PATH files/...
 *   /sdcard/Android/data/com.miniclip.bloons2/...  -> DATA_PATH external/...
 *   /data/data/com.miniclip.bloons2/...            -> DATA_PATH ...
 *   /sdcard/... , /mnt/sdcard/...                  -> DATA_PATH ...
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
