#include "settings.h"
#include "app.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

const char *settings_dir(void)
{
    static char *dir;
    if (!dir) dir = g_build_filename(g_get_user_config_dir(), APP_NAME, NULL);
    return dir;
}

static char *settings_path(void)
{
    return g_build_filename(settings_dir(), "settings.ini", NULL);
}

gboolean settings_exists(void)
{
    char *p = settings_path();
    gboolean ok = g_file_test(p, G_FILE_TEST_EXISTS);
    g_free(p);
    return ok;
}

static void defaults(Settings *s)
{
    memset(s, 0, sizeof *s);
    s->work_minutes = 20;
    s->break_seconds = 20;
    s->full_screen = TRUE;
    s->play_sound = TRUE;
    s->show_pet = TRUE;
    g_strlcpy(s->cat_name, "Mochi", sizeof s->cat_name);
    g_strlcpy(s->skin_name, "Tabby", sizeof s->skin_name);
    s->pet_scale = 4;
}

/* Accepts true/false and 1/0. */
static gboolean parse_bool(const char *v, gboolean *out)
{
    if (!g_ascii_strcasecmp(v, "true") || !strcmp(v, "1")) { *out = TRUE; return TRUE; }
    if (!g_ascii_strcasecmp(v, "false") || !strcmp(v, "0")) { *out = FALSE; return TRUE; }
    return FALSE;
}

static gboolean parse_int(const char *v, int *out)
{
    char *end;
    long n = strtol(v, &end, 10);
    if (end == v || *end) return FALSE;
    *out = (int)n;
    return TRUE;
}

void settings_load(Settings *s)
{
    defaults(s);
    char *path = settings_path(), *text = NULL;
    gboolean ok = g_file_get_contents(path, &text, NULL, NULL);
    g_free(path);
    if (!ok) return;

    char **lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        char *line = g_strstrip(*lp);
        char *eq = strchr(line, '=');
        if (!eq || eq == line || line[0] == ';' || line[0] == '[') continue;
        *eq = 0;
        char *k = g_strstrip(line), *v = g_strstrip(eq + 1);
        int n; gboolean b;
        if (!strcmp(k, "WorkMinutes")) { if (parse_int(v, &n) && n > 0) s->work_minutes = n; }
        else if (!strcmp(k, "BreakSeconds")) { if (parse_int(v, &n) && n > 0) s->break_seconds = n; }
        else if (!strcmp(k, "FullScreen")) { if (parse_bool(v, &b)) s->full_screen = b; }
        else if (!strcmp(k, "PlaySound")) { if (parse_bool(v, &b)) s->play_sound = b; }
        else if (!strcmp(k, "Paused")) { if (parse_bool(v, &b)) s->paused = b; }
        else if (!strcmp(k, "ShowPet")) { if (parse_bool(v, &b)) s->show_pet = b; }
        else if (!strcmp(k, "CatName")) { if (*v) { g_strlcpy(s->cat_name, v, sizeof s->cat_name); if (g_utf8_strlen(s->cat_name, -1) > 16) *g_utf8_offset_to_pointer(s->cat_name, 16) = 0; } }
        else if (!strcmp(k, "SkinName")) { if (*v) g_strlcpy(s->skin_name, v, sizeof s->skin_name); }
        else if (!strcmp(k, "PetScale")) { if (parse_int(v, &n)) s->pet_scale = CLAMP(n, 2, 8); }
        else if (!strcmp(k, "PetX")) { if (parse_int(v, &n)) s->pet_x = n; }
        else if (!strcmp(k, "PetY")) { if (parse_int(v, &n)) s->pet_y = n; }
        else if (!strcmp(k, "PetPlaced")) { if (parse_bool(v, &b)) s->pet_placed = b; }
    }
    g_strfreev(lines);
    g_free(text);
}

void settings_save(const Settings *s)
{
    g_mkdir_with_parents(settings_dir(), 0700);
    GString *sb = g_string_new(NULL);
    g_string_append_printf(sb, "WorkMinutes=%d\n", s->work_minutes);
    g_string_append_printf(sb, "BreakSeconds=%d\n", s->break_seconds);
    g_string_append_printf(sb, "FullScreen=%s\n", s->full_screen ? "True" : "False");
    g_string_append_printf(sb, "PlaySound=%s\n", s->play_sound ? "True" : "False");
    g_string_append_printf(sb, "Paused=%s\n", s->paused ? "True" : "False");
    g_string_append_printf(sb, "ShowPet=%s\n", s->show_pet ? "True" : "False");
    g_string_append_printf(sb, "CatName=%s\n", s->cat_name);
    g_string_append_printf(sb, "SkinName=%s\n", s->skin_name);
    g_string_append_printf(sb, "PetScale=%d\n", s->pet_scale);
    g_string_append_printf(sb, "PetX=%d\n", s->pet_x);
    g_string_append_printf(sb, "PetY=%d\n", s->pet_y);
    g_string_append_printf(sb, "PetPlaced=%s\n", s->pet_placed ? "True" : "False");
    char *path = settings_path();
    g_file_set_contents(path, sb->str, -1, NULL);
    g_free(path);
    g_string_free(sb, TRUE);
}

/* ---- autostart ---- */

static char *autostart_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "autostart", APP_ID ".desktop", NULL);
}

/* Path to relaunch: the AppImage if we are running from one, otherwise our own binary. */
static char *self_exe(void)
{
    const char *appimage = g_getenv("APPIMAGE");
    if (appimage && *appimage) return g_strdup(appimage);
    char *p = g_file_read_link("/proc/self/exe", NULL);
    return p ? p : g_strdup(APP_EXE);
}

gboolean settings_startup_enabled(void)
{
    char *p = autostart_path();
    gboolean ok = g_file_test(p, G_FILE_TEST_EXISTS);
    g_free(p);
    return ok;
}

void settings_set_startup(gboolean enabled)
{
    char *p = autostart_path();
    if (!enabled) {
        g_unlink(p);
    } else {
        char *dir = g_path_get_dirname(p);
        g_mkdir_with_parents(dir, 0700);
        g_free(dir);
        char *exe = self_exe();
        char *quoted = g_shell_quote(exe);
        char *text = g_strdup_printf(
            "[Desktop Entry]\nType=Application\nName=%s\nComment=Eye-break reminder\n"
            "Exec=%s --minimized\nIcon=%s\nTerminal=false\nCategories=Utility;\n"
            "X-GNOME-Autostart-enabled=true\nStartupNotify=false\n",
            APP_TITLE, quoted, APP_ID);
        g_file_set_contents(p, text, -1, NULL);
        g_free(text); g_free(quoted); g_free(exe);
    }
    g_free(p);
}
