#include "skin.h"
#include "settings.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#define SKIN_ERROR skin_error_quark()
static GQuark skin_error_quark(void) { return g_quark_from_static_string("skin-error"); }

const char *skin_dir(void)
{
    static char *dir;
    if (!dir) dir = g_build_filename(settings_dir(), "skins", NULL);
    return dir;
}

static void set_frame(Skin *s, const char *key, char **rows)
{
    char *k = g_ascii_strdown(key, -1);
    if (!g_hash_table_contains(s->frames, k)) g_ptr_array_add(s->frame_order, g_strdup(k));
    g_hash_table_insert(s->frames, k, rows);
}

static void set_palette(Skin *s, unsigned char c, Col col)
{
    if (!s->pal_set[c]) g_array_append_val(s->pal_order, c);
    s->pal_set[c] = TRUE;
    s->pal[c] = col;
}

char **skin_frame(const Skin *s, const char *key, gboolean alt)
{
    char *k = g_ascii_strdown(key, -1);
    char **r = NULL;
    if (alt) {
        char *k2 = g_strconcat(k, "2", NULL);
        r = g_hash_table_lookup(s->frames, k2);
        g_free(k2);
    }
    if (!r) r = g_hash_table_lookup(s->frames, k);
    if (!r && !strcmp(k, "sleep")) r = g_hash_table_lookup(s->frames, "blink");
    if (!r) r = g_hash_table_lookup(s->frames, "idle");
    g_free(k);
    return r;
}

gboolean skin_color(const Skin *s, char c, Col *out)
{
    unsigned char u = (unsigned char)c;
    if (!s->pal_set[u]) return FALSE;
    *out = s->pal[u];
    return TRUE;
}

static gboolean parse_color(const char *raw, Col *out, GError **err)
{
    const char *s = raw[0] == '#' ? raw + 1 : raw;
    char buf[9];
    size_t n = strlen(s);
    if (n == 3) { snprintf(buf, sizeof buf, "FF%c%c%c%c%c%c", s[0], s[0], s[1], s[1], s[2], s[2]); }
    else if (n == 6) { snprintf(buf, sizeof buf, "FF%s", s); }
    else if (n == 8) { snprintf(buf, sizeof buf, "%s", s); }
    else { buf[0] = 0; }
    char *end = NULL;
    unsigned long v = buf[0] ? strtoul(buf, &end, 16) : 0;
    if (!buf[0] || !end || *end) {
        g_set_error(err, SKIN_ERROR, 1, "Bad colour '%s' (use #RRGGBB or #AARRGGBB).", raw);
        return FALSE;
    }
    out->a = ((v >> 24) & 0xFF) / 255.0; out->r = ((v >> 16) & 0xFF) / 255.0;
    out->g = ((v >> 8) & 0xFF) / 255.0;  out->b = (v & 0xFF) / 255.0;
    return TRUE;
}

static Skin *skin_new(void)
{
    Skin *s = g_new0(Skin, 1);
    s->name = g_strdup("Unnamed");
    s->pal_order = g_array_new(FALSE, FALSE, sizeof(unsigned char));
    s->frames = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_strfreev);
    s->frame_order = g_ptr_array_new_with_free_func(g_free);
    return s;
}

void skin_free(Skin *s)
{
    if (!s || s->builtin) return;
    g_free(s->name); g_free(s->source);
    g_array_free(s->pal_order, TRUE);
    g_hash_table_destroy(s->frames);
    g_ptr_array_free(s->frame_order, TRUE);
    g_free(s);
}

static void flush(Skin *s, char *section, GPtrArray **rows)
{
    if (section && g_str_has_prefix(section, "frame ") && *rows && (*rows)->len > 0) {
        char *key = g_strstrip(g_strdup(section + 6));
        g_ptr_array_add(*rows, NULL);
        set_frame(s, key, (char **)g_ptr_array_free(*rows, FALSE));
        g_free(key);
        *rows = NULL;
    } else if (*rows) {
        g_ptr_array_free(*rows, TRUE);
        *rows = NULL;
    }
}

Skin *skin_parse(const char *text, const char *source, GError **err)
{
    Skin *s = skin_new();
    s->source = g_strdup(source);
    char *section = NULL;
    GPtrArray *rows = NULL;
    gboolean ok = TRUE;

    char **lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp && ok; lp++) {
        char *line = g_strchomp(*lp);                /* strips \r and trailing spaces */
        if (!*line || line[0] == '#') continue;
        char *lower = g_ascii_strdown(line, -1);

        if (g_str_has_prefix(lower, "name:")) {
            g_free(s->name); s->name = g_strstrip(g_strdup(line + 5));
        } else if (!strcmp(lower, "palette:")) {
            flush(s, section, &rows); g_free(section); section = g_strdup("palette");
        } else if (g_str_has_prefix(lower, "frame ") && g_str_has_suffix(lower, ":")) {
            flush(s, section, &rows); g_free(section);
            section = g_strstrip(g_strndup(line, strlen(line) - 1));
            rows = g_ptr_array_new_with_free_func(g_free);
        } else if (section && !strcmp(section, "palette")) {
            char **parts = g_strsplit_set(g_strstrip(line), " \t=", -1);
            const char *k = NULL, *v = NULL;
            for (char **p = parts; *p; p++) {
                if (!**p) continue;
                if (!k) k = *p; else if (!v) { v = *p; break; }
            }
            if (k && v && strlen(k) == 1) {
                Col c;
                if (parse_color(v, &c, err)) set_palette(s, (unsigned char)k[0], c); else ok = FALSE;
            }
            g_strfreev(parts);
        } else if (rows) {
            g_ptr_array_add(rows, g_strdup(g_strstrip(line)));
        }
        g_free(lower);
    }
    if (ok) flush(s, section, &rows); else if (rows) g_ptr_array_free(rows, TRUE);
    g_free(section);
    g_strfreev(lines);
    if (!ok) { skin_free(s); return NULL; }

    char **idle = g_hash_table_lookup(s->frames, "idle");
    if (!idle) {
        g_set_error(err, SKIN_ERROR, 2, "Skin needs at least a 'frame idle:' section.");
        skin_free(s); return NULL;
    }
    s->w = (int)strlen(idle[0]);
    s->h = (int)g_strv_length(idle);
    if (s->w < 4 || s->h < 4 || s->w > 64 || s->h > 64) {
        g_set_error(err, SKIN_ERROR, 3, "Frames must be between 4x4 and 64x64 pixels.");
        skin_free(s); return NULL;
    }
    GHashTableIter it; gpointer key, val;
    g_hash_table_iter_init(&it, s->frames);
    while (g_hash_table_iter_next(&it, &key, &val)) {
        char **f = val;
        int n = (int)g_strv_length(f);
        if (n != s->h) {
            g_set_error(err, SKIN_ERROR, 4, "Frame '%s' has %d rows, expected %d.", (char *)key, n, s->h);
            skin_free(s); return NULL;
        }
        for (int i = 0; i < n; i++) {
            if ((int)strlen(f[i]) != s->w) {
                g_set_error(err, SKIN_ERROR, 5, "Frame '%s' has a row of width %d, expected %d.", (char *)key, (int)strlen(f[i]), s->w);
                skin_free(s); return NULL;
            }
        }
    }
    return s;
}

char *skin_serialize(const Skin *s)
{
    GString *sb = g_string_new(NULL);
    g_string_append(sb, "# 20-20-20 skin. One character = one pixel. '.' (or any unlisted character) is transparent.\n");
    g_string_append(sb, "# Frames: idle (required), blink, far, happy, wide, sleep. Add '<name>2' for a second animation frame.\n");
    g_string_append(sb, "# All frames must be the same size. Any size from 4x4 to 64x64 works.\n");
    g_string_append_printf(sb, "name: %s\n\npalette:\n", s->name);
    for (guint i = 0; i < s->pal_order->len; i++) {
        unsigned char c = g_array_index(s->pal_order, unsigned char, i);
        Col k = s->pal[c];
        g_string_append_printf(sb, "%c #%02x%02x%02x\n", c, (int)(k.r * 255 + 0.5), (int)(k.g * 255 + 0.5), (int)(k.b * 255 + 0.5));
    }
    for (guint i = 0; i < s->frame_order->len; i++) {
        const char *key = g_ptr_array_index(s->frame_order, i);
        char **rows = g_hash_table_lookup(s->frames, key);
        g_string_append_printf(sb, "\nframe %s:\n", key);
        for (char **r = rows; r && *r; r++) g_string_append_printf(sb, "%s\n", *r);
    }
    return g_string_free(sb, FALSE);
}

/* ---------- discovery ---------- */

Skin *skin_load_by_name(const char *name)
{
    int n; const Skin *const *all = builtin_skins(&n);
    for (int i = 0; i < n; i++)
        if (!g_ascii_strcasecmp(all[i]->name, name)) return (Skin *)all[i];
    char *file = g_strconcat(name, SKIN_EXT, NULL);
    char *path = g_build_filename(skin_dir(), file, NULL);
    char *text = NULL;
    Skin *s = NULL;
    if (g_file_get_contents(path, &text, NULL, NULL)) s = skin_parse(text, path, NULL);
    g_free(text); g_free(path); g_free(file);
    return s ? s : (Skin *)all[0];
}

static gint cmp_nocase(gconstpointer a, gconstpointer b) { return g_ascii_strcasecmp(a, b); }

GList *skin_custom_names(void)
{
    GList *list = NULL;
    GDir *d = g_dir_open(skin_dir(), 0, NULL);
    if (!d) return NULL;
    const char *f;
    while ((f = g_dir_read_name(d))) {
        size_t n = strlen(f), e = strlen(SKIN_EXT);
        if (n > e && !g_ascii_strcasecmp(f + n - e, SKIN_EXT)) list = g_list_prepend(list, g_strndup(f, n - e));
    }
    g_dir_close(d);
    return g_list_sort(list, cmp_nocase);
}

/* Creates the skins folder with every built-in exported as an editable template. */
void skin_ensure_templates(void)
{
    g_mkdir_with_parents(skin_dir(), 0700);
    int n; const Skin *const *all = builtin_skins(&n);
    for (int i = 0; i < n; i++) {
        char *file = g_strconcat("_template-", all[i]->name, SKIN_EXT, NULL);
        char *p = g_build_filename(skin_dir(), file, NULL);
        if (!g_file_test(p, G_FILE_TEST_EXISTS)) {
            char *text = skin_serialize(all[i]);
            g_file_set_contents(p, text, -1, NULL);
            g_free(text);
        }
        g_free(p); g_free(file);
    }
    char *readme = g_build_filename(skin_dir(), "README.txt", NULL);
    if (!g_file_test(readme, G_FILE_TEST_EXISTS))
        g_file_set_contents(readme,
            "Custom skins for 20-20-20\n\n"
            "1. Copy one of the _template-*.skin files and rename it, e.g. MyCat.skin\n"
            "2. Open it in any text editor. Each character is one pixel.\n"
            "3. Change colours in the palette section (#RRGGBB) or add new letters.\n"
            "4. Edit the frames. Every frame must have the same width and height.\n"
            "5. Click the tray icon > Skin > pick your file.\n\n"
            "Files starting with _template- are regenerated if deleted; make a copy instead of editing them.\n"
            "Skin files are compatible with the Windows and macOS versions of 20-20-20.\n", -1, NULL);
    g_free(readme);
}

/* ---------- built-in skins, defined with the same text format so templates and code never drift ---------- */

/* Palettes: K outline, O fur, L light fur (muzzle/belly), P pink, E eye, W eye highlight */
static const char *PAL_TABBY    = "K #1b1b2f\nO #f0a04b\nL #fad38a\nP #f28ca8\nE #2a2a40\nW #ffffff";
static const char *PAL_MIDNIGHT = "K #0d0d18\nO #2e2e48\nL #4a4a6a\nP #f28ca8\nE #5ef0a0\nW #ffffff";
static const char *PAL_SNOW     = "K #3a3a4a\nO #f4f4f8\nL #e2e2ea\nP #f2a0b8\nE #4a90e2\nW #ffffff";
static const char *PAL_MATCHA   = "K #1b2f24\nO #7fcf9a\nL #c6f0d2\nP #f2a0b8\nE #2a2a40\nW #ffffff";

#define ROWS 20
static const char *BASE[ROWS] = {
    "...KK..........KK...",
    "..KOOK........KOOK..",
    "..KOPOK......KOPOK..",
    "..KOPPOKKKKKKOPPOK..",
    "..KOOOOOOOOOOOOOOK..",
    ".KOOOOOOOOOOOOOOOOK.",
    ".KOOOOOOOOOOOOOOOOK.",   /* 6 */
    ".KOOEEOOOOOOOOEEOOK.",   /* 7  eyes */
    ".KOOEWOOOOOOOOWEOOK.",   /* 8  eyes */
    ".KOOEEOOOLLOOOEEOOK.",   /* 9  eyes + muzzle */
    ".KOOOOOOLPLOOOOOOOK.",   /* 10 nose */
    ".KOOOOOOKLLKOOOOOOK.",   /* 11 mouth */
    "..KOOOOOOOOOOOOOOK..",
    "..KKOOOOOOOOOOOOKK..",   /* 13 */
    ".KOOOLLLLLLLLLLOOOK.",   /* 14 */
    ".KOOOLLLLLLLLLLOOOKK",   /* 15 tail */
    ".KOOOLLLLLLLLLLOOOOK",   /* 16 tail */
    ".KOOOLLLLLLLLLLOOOOK",   /* 17 tail */
    ".KOOKKOOOOOOKKOOOKK.",   /* 18 */
    ".KKK..KKKKKK..KKKK..",
};

typedef const char *Frame[ROWS];

static void patch(Frame dst, const Frame src, int first, int n, const char *const *rows)
{
    memcpy(dst, src, sizeof(Frame));
    for (int i = 0; i < n; i++) dst[first + i] = rows[i];
}

static void add(GString *sb, const char *key, const Frame rows)
{
    g_string_append_printf(sb, "frame %s:\n", key);
    for (int i = 0; i < ROWS; i++) g_string_append_printf(sb, "%s\n", rows[i]);
}

static Skin *make_cat(const char *name, const char *palette)
{
    Frame idle_alt, blink, far, far2, happy, happy2, wide;
    /* Tail swings up on the alternate frame. */
    static const char *const tail_up[] = {
        "..KOOOOOOOOOOOOOOK.K",
        "..KKOOOOOOOOOOOOKKOK",
        ".KOOOLLLLLLLLLLOOOKK",
        ".KOOOLLLLLLLLLLOOOK.",
        ".KOOOLLLLLLLLLLOOOK.",
        ".KOOOLLLLLLLLLLOOOK." };
    static const char *const blink_rows[] = {
        ".KOOOOOOOOOOOOOOOOK.",
        ".KOOKKOOOOOOOOKKOOK.",
        ".KOOOOOOOLLOOOOOOOK." };
    /* Gazing up into the distance: eyes shift one row up. */
    static const char *const far_rows[] = {
        ".KOOEEOOOOOOOOEEOOK.",
        ".KOOEWOOOOOOOOWEOOK.",
        ".KOOEEOOOOOOOOEEOOK.",
        ".KOOOOOOOLLOOOOOOOK." };
    /* ^ ^ eyes */
    static const char *const happy_rows[] = {
        ".KOOKKOOOOOOOOKKOOK.",
        ".KOKOOKOOOOOOKOOKOK.",
        ".KOOOOOOOLLOOOOOOOK." };
    /* Wide awake: taller eyes. */
    static const char *const wide_rows[] = {
        ".KOOEEOOOOOOOOEEOOK.",
        ".KOOEWOOOOOOOOWEOOK.",
        ".KOOEEOOOOOOOOEEOOK.",
        ".KOOEEOOOLLOOOEEOOK." };

    patch(idle_alt, BASE, 12, 6, tail_up);
    patch(blink, BASE, 7, 3, blink_rows);
    patch(far, BASE, 6, 4, far_rows);
    patch(far2, far, 12, 6, tail_up);
    patch(happy, BASE, 7, 3, happy_rows);
    patch(happy2, happy, 12, 6, tail_up);
    patch(wide, BASE, 6, 4, wide_rows);

    GString *sb = g_string_new(NULL);
    g_string_append_printf(sb, "name: %s\npalette:\n%s\n", name, palette);
    add(sb, "idle", BASE); add(sb, "idle2", idle_alt); add(sb, "blink", blink);
    add(sb, "far", far); add(sb, "far2", far2); add(sb, "happy", happy); add(sb, "happy2", happy2);
    add(sb, "wide", wide); add(sb, "sleep", blink);
    GError *err = NULL;
    Skin *s = skin_parse(sb->str, NULL, &err);
    if (!s) g_error("built-in skin %s is invalid: %s", name, err ? err->message : "?");
    s->builtin = TRUE;
    g_string_free(sb, TRUE);
    return s;
}

const Skin *const *builtin_skins(int *count)
{
    static Skin *all[4];
    if (!all[0]) {
        all[0] = make_cat("Tabby", PAL_TABBY);
        all[1] = make_cat("Midnight", PAL_MIDNIGHT);
        all[2] = make_cat("Snow", PAL_SNOW);
        all[3] = make_cat("Matcha", PAL_MATCHA);
    }
    *count = 4;
    return (const Skin *const *)all;
}
