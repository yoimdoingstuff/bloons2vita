/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <zlib.h>
#include <psp2/kernel/threadmgr.h>

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

// If `path` is `prefix` or starts with `prefix/`, write `replacement` +
// the rest of the path to `out` and return true.
static bool remap_prefix(const char *path, const char *prefix,
                         const char *replacement, char *out, size_t out_size) {
    size_t len = strlen(prefix);
    if (strncmp(path, prefix, len) != 0 || (path[len] != '\0' && path[len] != '/'))
        return false;

    const char *rest = path + len;
    if (*rest == '/') rest++;
    snprintf(out, out_size, "%s%s", replacement, rest);

    // "ux0:data/bloons2/" for the bare prefix -> drop the trailing slash
    size_t l = strlen(out);
    if (!*rest && l > 1 && out[l - 1] == '/')
        out[l - 1] = '\0';
    return true;
}

const char *remap_path(const char *path, char *out, size_t out_size) {
    if (!path)
        return NULL;

    if (strcmp(path, "/proc/cpuinfo") == 0)
        return "app0:/cpuinfo";
    if (strcmp(path, "/proc/meminfo") == 0)
        return "app0:/meminfo";
    if (strcmp(path, ANDROID_APK_PATH) == 0)
        return APK_PATH;

    if (remap_prefix(path, ANDROID_FILES_PATH, DATA_PATH "files/", out, out_size) ||
        remap_prefix(path, ANDROID_EXTERNAL_PATH, DATA_PATH "external/", out, out_size) ||
        remap_prefix(path, ANDROID_PKG_DATA_PATH, DATA_PATH, out, out_size) ||
        remap_prefix(path, "/mnt/sdcard", DATA_PATH, out, out_size) ||
        remap_prefix(path, "/sdcard", DATA_PATH, out, out_size))
        return out;

    return path;
}

#define REMAP(var, path) \
    char var##_buf[PATH_MAX]; \
    const char *var = remap_path((path), var##_buf, sizeof(var##_buf))

FILE * fopen_soloader(const char * filename, const char * mode) {
    REMAP(real, filename);
    FILE* ret = fopen(real, mode);

    if (!ret)
        l_debug("fopen(%s, %s): failed", real, mode);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    REMAP(real, path);
    int newlib_flags = oflags_bionic_to_newlib(oflag);
    int ret = open(real, newlib_flags, mode);
    l_debug("open(%s, %x): %i", real, newlib_flags, ret);
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    REMAP(real, path);
    struct stat st;
    int res = stat(real, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("stat(%s): %i", real, res);
    return res;
}

int lstat_soloader(const char * path, stat64_bionic * buf) {
    // No symlinks on the Vita
    return stat_soloader(path, buf);
}

int fclose_soloader(FILE * f) {
    return fclose(f);
}

int close_soloader(int fd) {
    return close(fd);
}

DIR* opendir_soloader(char* _pathname) {
    REMAP(real, _pathname);
    DIR* ret = opendir(real);
    l_debug("opendir(\"%s\"): %p", real, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    return ret;
}

int closedir_soloader(DIR * dir) {
    return closedir(dir);
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_debug("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_debug("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    return fsync(fd);
}

int access_soloader(const char *path, int mode) {
    REMAP(real, path);
    return access(real, mode);
}

int mkdir_soloader(const char *path, mode_t mode) {
    REMAP(real, path);
    int ret = mkdir(real, mode);
    l_debug("mkdir(%s): %i", real, ret);
    return ret;
}

int rmdir_soloader(const char *path) {
    REMAP(real, path);
    return rmdir(real);
}

int remove_soloader(const char *path) {
    REMAP(real, path);
    return remove(real);
}

int rename_soloader(const char *old_path, const char *new_path) {
    REMAP(real_old, old_path);
    REMAP(real_new, new_path);
    return rename(real_old, real_new);
}

int chdir_soloader(const char *path) {
    REMAP(real, path);
    return chdir(real);
}

int symlink_soloader(const char *target, const char *linkpath) {
    errno = EPERM;
    return -1;
}

ssize_t readlink_soloader(const char *path, char *buf, size_t bufsiz) {
    errno = EINVAL;
    return -1;
}

void *gzopen_soloader(const char *path, const char *mode) {
    REMAP(real, path);
    return gzopen(real, mode);
}
