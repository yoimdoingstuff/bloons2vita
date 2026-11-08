/*
 * apk.c
 *
 * Read-only access to the game's original .apk (a zip archive), standing in
 * for Android's AssetManager.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/apk.h"

#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>

#include "reimpl/io.h"
#include "utils/logger.h"
#include "utils/utils.h"

static zip_t *apk = NULL;
static pthread_mutex_t apk_lock = PTHREAD_MUTEX_INITIALIZER;

int apk_open(const char *path) {
    int err = 0;
    apk = zip_open(path, ZIP_RDONLY, &err);
    if (!apk) {
        l_error("apk_open(%s): zip_open failed with error %i", path, err);
        return -1;
    }
    l_success("apk_open(%s): %lli entries", path,
              (long long)zip_get_num_entries(apk, 0));
    return 0;
}

void apk_close(void) {
    pthread_mutex_lock(&apk_lock);
    if (apk) {
        zip_close(apk);
        apk = NULL;
    }
    pthread_mutex_unlock(&apk_lock);
}

// Must be called with apk_lock held.
static bool apk_read_entry(zip_int64_t idx, uint8_t **buf, size_t *size) {
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(apk, idx, 0, &st) != 0 || !(st.valid & ZIP_STAT_SIZE))
        return false;

    zip_file_t *zf = zip_fopen_index(apk, idx, 0);
    if (!zf)
        return false;

    // +1 so text assets can be treated as C strings if needed
    uint8_t *data = malloc(st.size + 1);
    if (!data) {
        zip_fclose(zf);
        l_error("apk: out of memory reading %s (%llu bytes)", st.name,
                (unsigned long long)st.size);
        return false;
    }

    zip_int64_t got = zip_fread(zf, data, st.size);
    zip_fclose(zf);

    if (got < 0 || (zip_uint64_t)got != st.size) {
        free(data);
        return false;
    }

    data[st.size] = '\0';
    *buf = data;
    *size = (size_t)st.size;
    return true;
}

// Must be called with apk_lock held.
static zip_int64_t apk_locate(const char *entry) {
    zip_int64_t idx = zip_name_locate(apk, entry, 0);
    if (idx < 0)
        idx = zip_name_locate(apk, entry, ZIP_FL_NOCASE);
    return idx;
}

// Must be called with apk_lock held.
static zip_int64_t apk_locate_asset(const char *name) {
    char entry[512];

    while (name[0] == '/' || (name[0] == '.' && name[1] == '/'))
        name += (name[0] == '/') ? 1 : 2;

    if (strncmp(name, "assets/", 7) == 0) {
        zip_int64_t idx = apk_locate(name);
        if (idx >= 0) return idx;
    }

    snprintf(entry, sizeof(entry), "assets/%s", name);
    zip_int64_t idx = apk_locate(entry);
    if (idx >= 0) return idx;

    const char *res = strstr(name, "Contents/Resources/");
    if (res) {
        snprintf(entry, sizeof(entry), "assets/%s",
                 res + strlen("Contents/Resources/"));
        idx = apk_locate(entry);
        if (idx >= 0) return idx;
    }

    const char *base = strrchr(name, '/');
    if (base) {
        snprintf(entry, sizeof(entry), "assets/%s", base + 1);
        idx = apk_locate(entry);
        if (idx >= 0) return idx;
    }

    return -1;
}

bool asset_load(const char *name, uint8_t **buf, size_t *size) {
    if (!name || !*name || !buf || !size)
        return false;

    bool ok = false;

    pthread_mutex_lock(&apk_lock);
    if (apk) {
        zip_int64_t idx = apk_locate_asset(name);
        if (idx >= 0)
            ok = apk_read_entry(idx, buf, size);
    }
    pthread_mutex_unlock(&apk_lock);

    if (!ok) {
        // A real file, given either as a Vita path or an Android one
        char path_buf[512];
        const char *real = remap_path(name, path_buf, sizeof(path_buf));
        if (strchr(real, ':') && file_exists(real))
            ok = file_load(real, buf, size);
    }

    if (!ok)
        l_warn("asset_load(\"%s\"): not found", name);

    return ok;
}

int apk_extract_dir(const char *prefix, const char *dest_dir) {
    if (!apk)
        return -1;

    size_t prefix_len = strlen(prefix);
    int written = 0;

    pthread_mutex_lock(&apk_lock);

    zip_int64_t n = zip_get_num_entries(apk, 0);
    for (zip_int64_t i = 0; i < n; i++) {
        const char *entry = zip_get_name(apk, i, 0);
        if (!entry || strncmp(entry, prefix, prefix_len) != 0)
            continue;

        const char *rel = entry + prefix_len;
        if (!*rel || rel[strlen(rel) - 1] == '/')
            continue; // directory entry

        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", dest_dir, rel);

        zip_stat_t st;
        zip_stat_init(&st);
        if (zip_stat_index(apk, i, 0, &st) == 0 && file_exists(out_path)
            && file_size(out_path) == (size_t)st.size) {
            continue; // already extracted
        }

        uint8_t *data;
        size_t size;
        if (!apk_read_entry(i, &data, &size)) {
            l_error("apk_extract_dir: failed to read %s", entry);
            continue;
        }

        file_mkpath(out_path, 0777);
        if (file_save(out_path, data, size))
            written++;
        else
            l_error("apk_extract_dir: failed to write %s", out_path);

        free(data);
    }

    pthread_mutex_unlock(&apk_lock);

    l_info("apk_extract_dir(%s -> %s): %i file(s) written", prefix, dest_dir,
           written);
    return written;
}
