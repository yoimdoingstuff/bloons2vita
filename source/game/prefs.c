/*
 * prefs.c
 *
 * Minimal persistent key/value store standing in for the game's
 * SharedPreferences("GAME_INFO").
 *
 * File format: one entry per line, "<type>\t<key>\t<value>\n", where type is
 * 'i' (int) or 's' (string). Tabs, newlines and backslashes in keys/values
 * are backslash-escaped.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/prefs.h"

#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/logger.h"
#include "utils/utils.h"

typedef struct pref_entry {
    char *key;
    char *str;   // NULL for ints
    int ival;
    struct pref_entry *next;
} pref_entry;

static pref_entry *prefs = NULL;
static char prefs_path[256];
static pthread_mutex_t prefs_lock = PTHREAD_MUTEX_INITIALIZER;

static pref_entry *find(const char *key) {
    for (pref_entry *e = prefs; e; e = e->next)
        if (strcmp(e->key, key) == 0)
            return e;
    return NULL;
}

static pref_entry *find_or_add(const char *key) {
    pref_entry *e = find(key);
    if (!e) {
        e = calloc(1, sizeof(pref_entry));
        e->key = strdup(key);
        e->next = prefs;
        prefs = e;
    }
    return e;
}

static void write_escaped(FILE *f, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '\\': fputs("\\\\", f); break;
            case '\t': fputs("\\t", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            default: fputc(*s, f); break;
        }
    }
}

static void unescape(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && r[1]) {
            r++;
            switch (*r) {
                case 't': *w++ = '\t'; break;
                case 'n': *w++ = '\n'; break;
                case 'r': *w++ = '\r'; break;
                default: *w++ = *r; break;
            }
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

// Must be called with prefs_lock held.
static void save(void) {
    char tmp_path[sizeof(prefs_path) + 4];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", prefs_path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        l_error("prefs: could not write %s", tmp_path);
        return;
    }

    for (pref_entry *e = prefs; e; e = e->next) {
        fputc(e->str ? 's' : 'i', f);
        fputc('\t', f);
        write_escaped(f, e->key);
        fputc('\t', f);
        if (e->str)
            write_escaped(f, e->str);
        else
            fprintf(f, "%d", e->ival);
        fputc('\n', f);
    }
    fclose(f);

    // Write-then-rename so a crash mid-save can't wipe the player's progress.
    remove(prefs_path);
    rename(tmp_path, prefs_path);
}

void prefs_load(const char *path) {
    pthread_mutex_lock(&prefs_lock);
    snprintf(prefs_path, sizeof(prefs_path), "%s", path);

    uint8_t *buf;
    size_t size;
    if (!file_exists(path) || !file_load(path, &buf, &size)) {
        l_info("prefs: no saved preferences at %s", path);
        pthread_mutex_unlock(&prefs_lock);
        return;
    }

    char *data = realloc(buf, size + 1);
    data[size] = '\0';

    char *saveptr = NULL;
    for (char *line = strtok_r(data, "\n", &saveptr); line;
         line = strtok_r(NULL, "\n", &saveptr)) {
        char *t1 = strchr(line, '\t');
        if (!t1) continue;
        char *t2 = strchr(t1 + 1, '\t');
        if (!t2) continue;
        *t1 = '\0';
        *t2 = '\0';

        char type = line[0];
        char *key = t1 + 1;
        char *val = t2 + 1;
        size_t vlen = strlen(val);
        if (vlen && val[vlen - 1] == '\r') val[vlen - 1] = '\0';
        unescape(key);
        unescape(val);

        pref_entry *e = find_or_add(key);
        free(e->str);
        e->str = NULL;
        if (type == 's')
            e->str = strdup(val);
        else
            e->ival = (int)strtol(val, NULL, 10);
    }

    free(data);
    l_success("prefs: loaded %s", path);
    pthread_mutex_unlock(&prefs_lock);
}

int prefs_get_int(const char *key, int def) {
    pthread_mutex_lock(&prefs_lock);
    pref_entry *e = find(key);
    int ret = (e && !e->str) ? e->ival : def;
    pthread_mutex_unlock(&prefs_lock);
    return ret;
}

void prefs_set_int(const char *key, int value) {
    pthread_mutex_lock(&prefs_lock);
    pref_entry *e = find_or_add(key);
    free(e->str);
    e->str = NULL;
    e->ival = value;
    save();
    pthread_mutex_unlock(&prefs_lock);
}

const char *prefs_get_string(const char *key, const char *def) {
    pthread_mutex_lock(&prefs_lock);
    pref_entry *e = find(key);
    const char *ret = (e && e->str) ? e->str : def;
    pthread_mutex_unlock(&prefs_lock);
    return ret;
}

void prefs_set_string(const char *key, const char *value) {
    pthread_mutex_lock(&prefs_lock);
    pref_entry *e = find_or_add(key);
    char *old = e->str;
    e->str = strdup(value ? value : "");
    free(old);
    save();
    pthread_mutex_unlock(&prefs_lock);
}
