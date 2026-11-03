#ifndef SKIN_H
#define SKIN_H

#include <glib.h>

/* A skin is a plain text file: a palette (one character = one colour) and a set of named
 * frames where every character is a single pixel. Users drop *.skin files into
 * ~/.config/TwentyTwentyTwenty/skins and pick them from the tray menu.
 *
 *   name: Tabby
 *   palette:
 *   K #1b1b2f
 *   O #f0a04b
 *   frame idle:
 *   ....KK....
 *   (rows...)
 *
 * Frame keys the app looks for (all optional except idle):
 *   idle, blink, far, happy, wide, sleep   and an optional "<key>2" alternate for animation
 *   (e.g. idle2 with the tail in a different position). Missing frames fall back to idle.
 *
 * The format is identical to the Windows and macOS builds, so skin files can be shared.
 */

typedef struct { double r, g, b, a; } Col;

typedef struct {
    char *name;
    char *source;              /* file path, or NULL for built-in */
    gboolean builtin;
    int w, h;
    Col pal[256];
    gboolean pal_set[256];
    GArray *pal_order;         /* of unsigned char */
    GHashTable *frames;        /* lowercased key -> char** rows (NULL-terminated) */
    GPtrArray *frame_order;    /* of char* */
} Skin;

#define SKIN_EXT ".skin"

const char *skin_dir(void);

Skin *skin_parse(const char *text, const char *source, GError **err);
void skin_free(Skin *s);                      /* no-op for built-ins */
char **skin_frame(const Skin *s, const char *key, gboolean alt);
gboolean skin_color(const Skin *s, char c, Col *out);
char *skin_serialize(const Skin *s);

/* Returns a built-in (do not free) or a freshly parsed custom skin. Falls back to the first built-in. */
Skin *skin_load_by_name(const char *name);
GList *skin_custom_names(void);               /* sorted; free with g_list_free_full(l, g_free) */
void skin_ensure_templates(void);

const Skin *const *builtin_skins(int *count);

#endif
