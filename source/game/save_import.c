/*
 * save_import.c
 *
 * One-time import of the player's own Android save data into the port.
 *
 * Expected layout on the memory card (copied from the phone's
 * /data/data/com.miniclip.bloons2/ folder):
 *
 *   ux0:data/bloons2/import/shared_prefs/GAME_INFO.xml   SharedPreferences
 *   ux0:data/bloons2/import/files/...                    game save files
 *
 * GAME_INFO.xml is Android's standard SharedPreferences XML:
 *
 *   <map>
 *     <int name="FULL_VERSION" value="1" />
 *     <string name="Device Id">...</string>
 *     ...
 *   </map>
 *
 * Its entries are merged into our preferences (game/prefs.c), and files/ is
 * copied over DATA_PATH "files/". Afterwards the folder is renamed to
 * "import_done" so the import only ever runs once.
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE file for details.
 */

#include "game/save_import.h"

#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/io/stat.h>

#include "game/prefs.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include "utils/utils.h"

#define IMPORT_DIR      DATA_PATH "import"
#define IMPORT_DONE_DIR DATA_PATH "import_done"

/*
 * SharedPreferences XML
 */

// Decode XML entities in place (&amp; &lt; &gt; &quot; &apos; &#N; &#xN;)
static void xml_unescape(char *s) {
    char *w = s;
    for (char *r = s; *r;) {
        if (*r != '&') { *w++ = *r++; continue; }
        if (strncmp(r, "&amp;", 5) == 0)       { *w++ = '&';  r += 5; }
        else if (strncmp(r, "&lt;", 4) == 0)   { *w++ = '<';  r += 4; }
        else if (strncmp(r, "&gt;", 4) == 0)   { *w++ = '>';  r += 4; }
        else if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"';  r += 6; }
        else if (strncmp(r, "&apos;", 6) == 0) { *w++ = '\''; r += 6; }
        else if (r[1] == '#') {
            char *end;
            long cp = (r[2] == 'x' || r[2] == 'X') ? strtol(r + 3, &end, 16) : strtol(r + 2, &end, 10);
            if (*end != ';') { *w++ = *r++; continue; }
            r = end + 1;
            // encode as UTF-8
            if (cp < 0x80) { *w++ = (char)cp; }
            else if (cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6)); *w++ = (char)(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { *w++ = (char)(0xE0 | (cp >> 12)); *w++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *w++ = (char)(0x80 | (cp & 0x3F)); }
            else { *w++ = (char)(0xF0 | (cp >> 18)); *w++ = (char)(0x80 | ((cp >> 12) & 0x3F)); *w++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *w++ = (char)(0x80 | (cp & 0x3F)); }
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

// Copy the value of attribute `attr` from the tag text into `out`.
static bool xml_attr(const char *tag, const char *attr, char *out, size_t out_size) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "%s=", attr);
    const char *p = tag;
    while ((p = strstr(p, pattern))) {
        // must be a whole attribute name
        if (p == tag || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r') {
            p += strlen(pattern);
            char quote = *p;
            if (quote != '"' && quote != '\'') return false;
            const char *end = strchr(p + 1, quote);
            if (!end) return false;
            size_t n = (size_t)(end - (p + 1));
            if (n >= out_size) n = out_size - 1;
            memcpy(out, p + 1, n);
            out[n] = '\0';
            xml_unescape(out);
            return true;
        }
        p += strlen(pattern);
    }
    return false;
}

static int import_shared_prefs(const char *path) {
    uint8_t *buf;
    size_t size;
    if (!file_load(path, &buf, &size)) return -1;
    char *xml = realloc(buf, size + 1);
    xml[size] = '\0';

    int imported = 0;
    char name[256], value[1024];

    for (char *p = strchr(xml, '<'); p; p = strchr(p + 1, '<')) {
        char *tag_end = strchr(p, '>');
        if (!tag_end) break;

        bool is_string = strncmp(p, "<string ", 8) == 0;
        bool is_int = strncmp(p, "<int ", 5) == 0;
        bool is_bool = strncmp(p, "<boolean ", 9) == 0;
        bool is_long = strncmp(p, "<long ", 6) == 0;

        if (!is_string && !is_int && !is_bool && !is_long) continue;

        // Work on a copy of the opening tag
        size_t tag_len = (size_t)(tag_end - p + 1);
        char *tag = malloc(tag_len + 1);
        memcpy(tag, p, tag_len);
        tag[tag_len] = '\0';

        if (!xml_attr(tag, "name", name, sizeof(name))) {
            free(tag);
            continue;
        }

        if (is_string) {
            if (tag_end[-1] == '/') {
                value[0] = '\0'; // <string name="x" />
            } else {
                char *close = strstr(tag_end + 1, "</string>");
                if (!close) { free(tag); break; }
                size_t n = (size_t)(close - (tag_end + 1));
                if (n >= sizeof(value)) n = sizeof(value) - 1;
                memcpy(value, tag_end + 1, n);
                value[n] = '\0';
                xml_unescape(value);
            }
            prefs_set_string(name, value);
            imported++;
        } else if (xml_attr(tag, "value", value, sizeof(value))) {
            int v;
            if (is_bool)
                v = strcmp(value, "true") == 0 ? 1 : 0;
            else
                v = (int)strtoll(value, NULL, 10);
            prefs_set_int(name, v);
            imported++;
        }

        free(tag);
    }

    free(xml);
    return imported;
}

/*
 * files/
 */

static int copy_tree(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) return 0;

    int copied = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        char s[512], t[512];
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);

        if (SCE_S_ISDIR(e->d_stat.st_mode)) {
            copied += copy_tree(s, t);
        } else if (file_copy(s, t)) {
            copied++;
        } else {
            l_error("save_import: failed to copy %s", s);
        }
    }

    closedir(d);
    return copied;
}

void save_import_run(void) {
    if (!is_dir(IMPORT_DIR))
        return;

    l_info("save_import: importing Android save data from %s", IMPORT_DIR);

    int prefs = import_shared_prefs(IMPORT_DIR "/shared_prefs/GAME_INFO.xml");
    int files = copy_tree(IMPORT_DIR "/files", DATA_PATH "files");

    if (prefs < 0 && files == 0) {
        warning("Found ux0:data/bloons2/import/ but nothing to import in it. "
                "Expected shared_prefs/GAME_INFO.xml and/or a files/ folder "
                "copied from your phone.");
        return;
    }

    // Only import once
    if (is_dir(IMPORT_DONE_DIR)) {
        char old[256];
        snprintf(old, sizeof(old), "%s_%llu", IMPORT_DONE_DIR,
                 (unsigned long long)current_timestamp_ms());
        rename(IMPORT_DONE_DIR, old);
    }
    rename(IMPORT_DIR, IMPORT_DONE_DIR);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Imported your Android save: %i setting(s) and %i file(s).",
             prefs < 0 ? 0 : prefs, files);
    l_success("save_import: %s", msg);
    warning(msg);
}
