#include "tray.h"
#include "app.h"
#include "settings.h"
#include "engine.h"
#include "skin.h"
#include "pixel.h"
#include "native.h"
#include "pet.h"
#include "overlay.h"
#include <string.h>
#include <stdio.h>

#if defined(HAVE_LEGACY_APPINDICATOR)
#include <libappindicator/app-indicator.h>
#elif defined(HAVE_APPINDICATOR)
#include <libayatana-appindicator/app-indicator.h>
#endif

#define TRAY_ICON_PX 44   /* 2x a 22px panel icon; panels downscale 2:1 which keeps pixels crisp */

struct Tray {
    GtkApplication *app;
    Settings s;
    Engine *eng;
    Skin *skin;

    GtkWidget *menu, *skin_menu;
    GtkWidget *status_item, *pause_item, *pet_item, *rename_item, *fullscreen_item, *sound_item, *startup_item;
    gboolean updating;
    int icon_state;   /* 0 unset, 1 active, 2 paused */

#ifdef HAVE_APPINDICATOR
    AppIndicator *ind;
    char *icon_dir, *icon_active_name, *icon_paused_name;
#else
    GtkStatusIcon *icon;
    GdkPixbuf *icon_active, *icon_paused;
#endif

    Pet *pet;
    BreakSession *brk;
};

static void update_ui(Tray *t);
static void recreate_pet(Tray *t);
static void populate_skin_menu(Tray *t);

/* ---------- icons ---------- */

static void rebuild_icons(Tray *t)
{
    GdkPixbuf *active = px_pixbuf(t->skin, skin_frame(t->skin, "idle", FALSE), TRAY_ICON_PX);
    GdkPixbuf *paused = px_pixbuf(t->skin, skin_frame(t->skin, "sleep", FALSE), TRAY_ICON_PX);
#ifdef HAVE_APPINDICATOR
    /* AppIndicator wants icon *names* resolved from a theme path, and caches by name: so the
       name includes a hash of the skin and the files live in the cache dir. */
    if (!t->icon_dir) {
        t->icon_dir = g_build_filename(g_get_user_cache_dir(), APP_NAME, "tray", NULL);
        g_mkdir_with_parents(t->icon_dir, 0700);
    }
    char *ser = skin_serialize(t->skin);
    char *sum = g_compute_checksum_for_string(G_CHECKSUM_MD5, ser, -1);
    sum[10] = 0;
    g_free(t->icon_active_name); g_free(t->icon_paused_name);
    t->icon_active_name = g_strdup_printf("tt-%s-active", sum);
    t->icon_paused_name = g_strdup_printf("tt-%s-paused", sum);
    char *pa = g_strdup_printf("%s/%s.png", t->icon_dir, t->icon_active_name);
    char *pp = g_strdup_printf("%s/%s.png", t->icon_dir, t->icon_paused_name);
    gdk_pixbuf_save(active, pa, "png", NULL, NULL);
    gdk_pixbuf_save(paused, pp, "png", NULL, NULL);
    g_free(pa); g_free(pp); g_free(sum); g_free(ser);
    g_object_unref(active); g_object_unref(paused);
    t->icon_state = 0;
#else
    g_clear_object(&t->icon_active); g_clear_object(&t->icon_paused);
    t->icon_active = active; t->icon_paused = paused;
    t->icon_state = 0;
#endif
}

gboolean tray_write_icons(const char *dir)
{
    int n; const Skin *const *all = builtin_skins(&n);
    const Skin *skin = all[0];
    char **rows = skin_frame(skin, "idle", FALSE);
    if (g_mkdir_with_parents(dir, 0755) != 0) return FALSE;
    GdkPixbuf *big = px_pixbuf(skin, rows, 512);
    static const int sizes[] = { 16, 22, 24, 32, 48, 64, 128, 256, 512 };
    gboolean ok = TRUE;
    for (size_t i = 0; i < G_N_ELEMENTS(sizes); i++) {
        int sz = sizes[i];
        GdkPixbuf *pb = sz >= 2 * MAX(skin->w, skin->h) ? px_pixbuf(skin, rows, sz)
                                                         : gdk_pixbuf_scale_simple(big, sz, sz, GDK_INTERP_HYPER);
        char *path = g_strdup_printf("%s/%d.png", dir, sz);
        ok = gdk_pixbuf_save(pb, path, "png", NULL, NULL) && ok;
        g_free(path);
        g_object_unref(pb);
    }
    g_object_unref(big);
    return ok;
}

/* ---------- dialogs ---------- */

static void prompt_name(Tray *t, gboolean welcome)
{
    GtkWidget *d = gtk_dialog_new_with_buttons(welcome ? "Meet your eye-care buddy" : "Rename your cat", NULL,
                                               GTK_DIALOG_MODAL, "_OK", GTK_RESPONSE_OK,
                                               welcome ? NULL : "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    gtk_window_set_keep_above(GTK_WINDOW(d), TRUE);
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    if (welcome) {
        GdkPixbuf *pb = px_pixbuf(t->skin, skin_frame(t->skin, "happy", FALSE), 80);
        gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
        g_object_unref(pb);
    }
    GtkWidget *label = gtk_label_new(welcome
        ? "A pixel cat will sit in your tray (and on your desktop) and remind you to look\n"
          "20 feet away for 20 seconds, every 20 minutes.\n\n"
          "What would you like to call it? Everything else can be changed from the tray menu."
        : "What should the cat be called?");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 16);
    gtk_entry_set_text(GTK_ENTRY(entry), t->s.cat_name);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(d))), box);
    gtk_widget_show_all(d);
    gtk_widget_grab_focus(entry);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        char *n = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))));
        if (*n) g_strlcpy(t->s.cat_name, n, sizeof t->s.cat_name);
        g_free(n);
    }
    gtk_widget_destroy(d);
}

static void refresh_names(Tray *t)
{
    char buf[128];
    snprintf(buf, sizeof buf, "Show %s on desktop", t->s.cat_name);
    gtk_menu_item_set_label(GTK_MENU_ITEM(t->pet_item), buf);
    snprintf(buf, sizeof buf, "Rename %s...", t->s.cat_name);
    gtk_menu_item_set_label(GTK_MENU_ITEM(t->rename_item), buf);
    update_ui(t);
    if (t->pet) pet_redraw(t->pet);
}

/* ---------- menu actions ---------- */

static void on_pause(GtkMenuItem *i, gpointer ud) { (void)i; Tray *t = ud; engine_toggle_pause(t->eng); }
static void on_break_now(GtkMenuItem *i, gpointer ud) { (void)i; Tray *t = ud; engine_request_break(t->eng); }
static void on_reset(GtkMenuItem *i, gpointer ud) { (void)i; Tray *t = ud; engine_reset(t->eng); }
static void on_quit(GtkMenuItem *i, gpointer ud) { (void)i; Tray *t = ud; g_application_quit(G_APPLICATION(t->app)); }

static void on_rename(GtkMenuItem *i, gpointer ud)
{
    (void)i; Tray *t = ud;
    prompt_name(t, FALSE);
    settings_save(&t->s);
    refresh_names(t);
}

static void on_pet_toggled(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating) return;
    t->s.show_pet = gtk_check_menu_item_get_active(i);
    settings_save(&t->s);
    recreate_pet(t);
}

static void on_fullscreen_toggled(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating) return;
    t->s.full_screen = gtk_check_menu_item_get_active(i);
    settings_save(&t->s);
}

static void on_sound_toggled(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating) return;
    t->s.play_sound = gtk_check_menu_item_get_active(i);
    settings_save(&t->s);
}

static void on_startup_toggled(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating) return;
    settings_set_startup(gtk_check_menu_item_get_active(i));
    t->updating = TRUE;
    gtk_check_menu_item_set_active(i, settings_startup_enabled());
    t->updating = FALSE;
}

static void on_interval(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating || !gtk_check_menu_item_get_active(i)) return;
    t->s.work_minutes = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(i), "value"));
    settings_save(&t->s);
    engine_interval_changed(t->eng);
}

static void on_break_len(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating || !gtk_check_menu_item_get_active(i)) return;
    t->s.break_seconds = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(i), "value"));
    settings_save(&t->s);
}

static void on_pet_size(GtkCheckMenuItem *i, gpointer ud)
{
    Tray *t = ud;
    if (t->updating || !gtk_check_menu_item_get_active(i)) return;
    t->s.pet_scale = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(i), "value"));
    t->s.pet_placed = FALSE;
    settings_save(&t->s);
    recreate_pet(t);
}

static GtkWidget *radio_submenu(Tray *t, const char *const *labels, const int *values, int n, int current, GCallback cb)
{
    GtkWidget *sub = gtk_menu_new();
    GSList *group = NULL;
    t->updating = TRUE;
    for (int i = 0; i < n; i++) {
        GtkWidget *it = gtk_radio_menu_item_new_with_label(group, labels[i]);
        group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(it));
        g_object_set_data(G_OBJECT(it), "value", GINT_TO_POINTER(values[i]));
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), values[i] == current);
        g_signal_connect(it, "toggled", cb, t);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), it);
    }
    t->updating = FALSE;
    return sub;
}

static GtkWidget *check_item(Tray *t, const char *label, gboolean active, GCallback cb)
{
    GtkWidget *it = gtk_check_menu_item_new_with_label(label);
    t->updating = TRUE;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), active);
    t->updating = FALSE;
    g_signal_connect(it, "toggled", cb, t);
    return it;
}

static GtkWidget *action_item(Tray *t, const char *label, GCallback cb)
{
    GtkWidget *it = gtk_menu_item_new_with_label(label);
    g_signal_connect(it, "activate", cb, t);
    return it;
}

static GtkWidget *submenu_item(const char *label, GtkWidget *sub)
{
    GtkWidget *it = gtk_menu_item_new_with_label(label);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(it), sub);
    return it;
}

/* ---------- skins ---------- */

static void apply_skin(Tray *t, const char *name)
{
    char *file = g_strconcat(name, SKIN_EXT, NULL);
    char *path = g_build_filename(skin_dir(), file, NULL);
    Skin *loaded = NULL;
    GError *err = NULL;
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        char *text = NULL;
        if (g_file_get_contents(path, &text, NULL, &err)) loaded = skin_parse(text, path, &err);
        g_free(text);
    } else {
        loaded = skin_load_by_name(name);
    }
    g_free(path); g_free(file);
    if (!loaded) {
        char *msg = g_strdup_printf("Couldn't load skin '%s':\n\n%s", name, err ? err->message : "unknown error");
        native_alert(NULL, GTK_MESSAGE_WARNING, msg);
        g_free(msg); g_clear_error(&err);
        return;
    }
    Skin *old = t->skin;
    t->skin = loaded;
    g_strlcpy(t->s.skin_name, name, sizeof t->s.skin_name);
    settings_save(&t->s);
    rebuild_icons(t);
    update_ui(t);
    recreate_pet(t);
    skin_free(old);
}

static void on_skin_pick(GtkMenuItem *i, gpointer ud) { Tray *t = ud; apply_skin(t, g_object_get_data(G_OBJECT(i), "skin")); }

static void on_open_skins(GtkMenuItem *i, gpointer ud)
{
    (void)i; (void)ud;
    skin_ensure_templates();
    char *uri = g_filename_to_uri(skin_dir(), NULL, NULL);
    if (uri) gtk_show_uri_on_window(NULL, uri, GDK_CURRENT_TIME, NULL);
    g_free(uri);
}

static void add_skin_item(Tray *t, const char *name)
{
    GtkWidget *it = gtk_check_menu_item_new_with_label(name);
    gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(it), TRUE);
    t->updating = TRUE;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(it), !g_ascii_strcasecmp(t->s.skin_name, name));
    t->updating = FALSE;
    g_object_set_data_full(G_OBJECT(it), "skin", g_strdup(name), g_free);
    g_signal_connect(it, "activate", G_CALLBACK(on_skin_pick), t);
    gtk_menu_shell_append(GTK_MENU_SHELL(t->skin_menu), it);
}

static void populate_skin_menu(Tray *t)
{
    GList *kids = gtk_container_get_children(GTK_CONTAINER(t->skin_menu));
    for (GList *l = kids; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(kids);

    int n; const Skin *const *all = builtin_skins(&n);
    for (int i = 0; i < n; i++) add_skin_item(t, all[i]->name);
    GList *custom = skin_custom_names();
    gboolean any = FALSE;
    for (GList *l = custom; l; l = l->next) {
        if (g_str_has_prefix(l->data, "_template-")) continue;
        if (!any) { gtk_menu_shell_append(GTK_MENU_SHELL(t->skin_menu), gtk_separator_menu_item_new()); any = TRUE; }
        add_skin_item(t, l->data);
    }
    g_list_free_full(custom, g_free);
    gtk_menu_shell_append(GTK_MENU_SHELL(t->skin_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(t->skin_menu), action_item(t, "Open skins folder (make your own)...", G_CALLBACK(on_open_skins)));
    gtk_widget_show_all(t->skin_menu);
}

static void on_skin_select(GtkMenuItem *i, gpointer ud) { (void)i; populate_skin_menu(ud); }

/* ---------- pet / break ---------- */

static void toggle_pause_cb(void *ud) { Tray *t = ud; engine_toggle_pause(t->eng); }

static void recreate_pet(Tray *t)
{
    if (t->pet) { pet_free(t->pet); t->pet = NULL; }
    if (!t->s.show_pet) return;
    t->pet = pet_new(&t->s, t->eng, t->skin, t->menu, toggle_pause_cb, t);
}

static void on_break_done(gboolean completed, void *ud)
{
    Tray *t = ud;
    t->brk = NULL;
    engine_complete_break(t->eng, completed);
    if (t->pet) pet_set_visible(t->pet, TRUE);
}

static void start_break(void *ud)
{
    Tray *t = ud;
    if (t->brk) return;
    if (t->pet) pet_set_visible(t->pet, FALSE);
    t->brk = break_session_new(&t->s, t->skin, on_break_done, t);
}

static void on_engine_changed(void *ud)
{
    Tray *t = ud;
    update_ui(t);
    if (t->pet) pet_engine_changed(t->pet);
}

static void update_ui(Tray *t)
{
    Mood mood = engine_mood(t->eng);
    gboolean sleepy = mood == MOOD_SLEEPING || mood == MOOD_AWAY;
    char status[64], line[160];
    engine_status(t->eng, status, sizeof status);
    snprintf(line, sizeof line, "%s: %s", t->s.cat_name, status);
    gtk_menu_item_set_label(GTK_MENU_ITEM(t->status_item), line);
    gtk_menu_item_set_label(GTK_MENU_ITEM(t->pause_item), t->s.paused ? "Resume" : "Pause");
    snprintf(line, sizeof line, "%s - %s", APP_TITLE, status);
    int state = sleepy ? 2 : 1;
#ifdef HAVE_APPINDICATOR
    if (state != t->icon_state) app_indicator_set_icon_full(t->ind, sleepy ? t->icon_paused_name : t->icon_active_name, status);
    app_indicator_set_title(t->ind, line);
#else
    if (state != t->icon_state) gtk_status_icon_set_from_pixbuf(t->icon, sleepy ? t->icon_paused : t->icon_active);
    gtk_status_icon_set_tooltip_text(t->icon, line);
#endif
    t->icon_state = state;
}

#ifndef HAVE_APPINDICATOR
static void on_icon_popup(GtkStatusIcon *icon, guint button, guint time, gpointer ud)
{
    Tray *t = ud;
    gtk_menu_popup(GTK_MENU(t->menu), NULL, NULL, gtk_status_icon_position_menu, icon, button, time);
}
static void on_icon_activate(GtkStatusIcon *icon, gpointer ud)
{
    (void)icon; Tray *t = ud;
    gtk_menu_popup(GTK_MENU(t->menu), NULL, NULL, gtk_status_icon_position_menu, icon, 0, gtk_get_current_event_time());
}
#endif

/* ---------- lifecycle ---------- */

Tray *tray_new(GtkApplication *app)
{
    Tray *t = g_new0(Tray, 1);
    t->app = app;
    gboolean first_run = !settings_exists();
    settings_load(&t->s);
    t->eng = engine_new(&t->s);
    t->skin = skin_load_by_name(t->s.skin_name);
    rebuild_icons(t);

    /* --- menu --- */
    t->menu = gtk_menu_new();
    t->status_item = gtk_menu_item_new_with_label("");
    gtk_widget_set_sensitive(t->status_item, FALSE);
    t->pause_item = action_item(t, "Pause", G_CALLBACK(on_pause));

    static const char *const int_labels[] = { "10 min", "15 min", "20 min", "25 min", "30 min", "45 min", "60 min" };
    static const int int_values[] = { 10, 15, 20, 25, 30, 45, 60 };
    static const char *const brk_labels[] = { "20 sec", "30 sec", "45 sec", "60 sec" };
    static const int brk_values[] = { 20, 30, 45, 60 };
    static const char *const size_labels[] = { "Small", "Medium", "Large", "Huge" };
    static const int size_values[] = { 3, 4, 6, 8 };

    t->pet_item = check_item(t, "", t->s.show_pet, G_CALLBACK(on_pet_toggled));
    t->rename_item = action_item(t, "", G_CALLBACK(on_rename));
    t->skin_menu = gtk_menu_new();
    GtkWidget *skin_item = submenu_item("Skin", t->skin_menu);
    g_signal_connect(skin_item, "select", G_CALLBACK(on_skin_select), t);
    populate_skin_menu(t);
    t->fullscreen_item = check_item(t, "Full-screen break (all monitors)", t->s.full_screen, G_CALLBACK(on_fullscreen_toggled));
    t->sound_item = check_item(t, "Play sound", t->s.play_sound, G_CALLBACK(on_sound_toggled));
    t->startup_item = check_item(t, "Run at login", settings_startup_enabled(), G_CALLBACK(on_startup_toggled));

    GtkMenuShell *m = GTK_MENU_SHELL(t->menu);
    gtk_menu_shell_append(m, t->status_item);
    gtk_menu_shell_append(m, gtk_separator_menu_item_new());
    gtk_menu_shell_append(m, t->pause_item);
    gtk_menu_shell_append(m, action_item(t, "Take a break now", G_CALLBACK(on_break_now)));
    gtk_menu_shell_append(m, action_item(t, "Reset timer", G_CALLBACK(on_reset)));
    gtk_menu_shell_append(m, gtk_separator_menu_item_new());
    gtk_menu_shell_append(m, submenu_item("Work interval", radio_submenu(t, int_labels, int_values, 7, t->s.work_minutes, G_CALLBACK(on_interval))));
    gtk_menu_shell_append(m, submenu_item("Break length", radio_submenu(t, brk_labels, brk_values, 4, t->s.break_seconds, G_CALLBACK(on_break_len))));
    gtk_menu_shell_append(m, gtk_separator_menu_item_new());
    gtk_menu_shell_append(m, t->pet_item);
    gtk_menu_shell_append(m, submenu_item("Pet size", radio_submenu(t, size_labels, size_values, 4, t->s.pet_scale, G_CALLBACK(on_pet_size))));
    gtk_menu_shell_append(m, skin_item);
    gtk_menu_shell_append(m, t->rename_item);
    gtk_menu_shell_append(m, t->fullscreen_item);
    gtk_menu_shell_append(m, t->sound_item);
    gtk_menu_shell_append(m, t->startup_item);
    gtk_menu_shell_append(m, gtk_separator_menu_item_new());
    gtk_menu_shell_append(m, action_item(t, "Quit", G_CALLBACK(on_quit)));
    gtk_widget_show_all(t->menu);

    /* --- tray icon --- */
#ifdef HAVE_APPINDICATOR
    t->ind = app_indicator_new_with_path(APP_ID, t->icon_active_name, APP_INDICATOR_CATEGORY_APPLICATION_STATUS, t->icon_dir);
    app_indicator_set_status(t->ind, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(t->ind, GTK_MENU(t->menu));
    app_indicator_set_secondary_activate_target(t->ind, t->pause_item);   /* middle click = pause */
#else
    t->icon = gtk_status_icon_new();
    gtk_status_icon_set_title(t->icon, APP_TITLE);
    g_signal_connect(t->icon, "popup-menu", G_CALLBACK(on_icon_popup), t);
    g_signal_connect(t->icon, "activate", G_CALLBACK(on_icon_activate), t);
    gtk_status_icon_set_visible(t->icon, TRUE);
#endif

    t->eng->on_changed = on_engine_changed;
    t->eng->on_break_requested = start_break;
    t->eng->ud = t;
    refresh_names(t);

    if (first_run) {
        prompt_name(t, TRUE);
        settings_save(&t->s);
        refresh_names(t);
    }
    recreate_pet(t);
    return t;
}

void tray_free(Tray *t)
{
    if (!t) return;
    t->eng->on_changed = NULL;
    t->eng->on_break_requested = NULL;
    if (t->brk) { BreakSession *b = t->brk; t->brk = NULL; break_session_free(b); }
    if (t->pet) pet_free(t->pet);
    engine_free(t->eng);
#ifdef HAVE_APPINDICATOR
    g_clear_object(&t->ind);
    g_free(t->icon_dir); g_free(t->icon_active_name); g_free(t->icon_paused_name);
#else
    gtk_status_icon_set_visible(t->icon, FALSE);
    g_clear_object(&t->icon);
    g_clear_object(&t->icon_active); g_clear_object(&t->icon_paused);
#endif
    gtk_widget_destroy(t->menu);
    skin_free(t->skin);
    g_free(t);
}
