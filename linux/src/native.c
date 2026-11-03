#include "native.h"
#include "app.h"
#include <gio/gio.h>

#ifdef HAVE_XSS
#include <gdk/gdkx.h>
#include <X11/extensions/scrnsaver.h>

static int xss_idle_seconds(gboolean *supported)
{
    GdkDisplay *disp = gdk_display_get_default();
    if (!disp || !GDK_IS_X11_DISPLAY(disp)) { *supported = FALSE; return 0; }
    Display *d = GDK_DISPLAY_XDISPLAY(disp);
    static XScreenSaverInfo *info;
    static int checked, ok;
    if (!checked) { int ev, er; ok = XScreenSaverQueryExtension(d, &ev, &er); checked = 1; }
    if (!ok) { *supported = FALSE; return 0; }
    if (!info) info = XScreenSaverAllocInfo();
    *supported = TRUE;
    if (!XScreenSaverQueryInfo(d, DefaultRootWindow(d), info)) return 0;
    return (int)(info->idle / 1000);
}
#endif

/* Mutter idle monitor: asynchronous so a slow or missing service never blocks the UI. */
static guint64 dbus_idle_ms;
static gboolean dbus_idle_failed, dbus_idle_pending;

static void idle_reply(GObject *src, GAsyncResult *res, gpointer ud)
{
    (void)ud;
    GError *err = NULL;
    GVariant *v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    dbus_idle_pending = FALSE;
    if (!v) { dbus_idle_failed = TRUE; g_clear_error(&err); return; }
    g_variant_get(v, "(t)", &dbus_idle_ms);
    g_variant_unref(v);
}

static int dbus_idle_seconds(void)
{
    if (dbus_idle_failed) return 0;
    if (!dbus_idle_pending) {
        GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        if (!bus) { dbus_idle_failed = TRUE; return 0; }
        dbus_idle_pending = TRUE;
        g_dbus_connection_call(bus, "org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core",
                               "org.gnome.Mutter.IdleMonitor", "GetIdletime", NULL, G_VARIANT_TYPE("(t)"),
                               G_DBUS_CALL_FLAGS_NONE, 800, NULL, idle_reply, NULL);
        g_object_unref(bus);
    }
    return (int)(dbus_idle_ms / 1000);
}

int native_idle_seconds(void)
{
#ifdef HAVE_XSS
    gboolean supported;
    int s = xss_idle_seconds(&supported);
    if (supported) return s;
#endif
    return dbus_idle_seconds();
}

void native_play_sound(const char *event_id)
{
    static int have_canberra = -1;
    if (have_canberra < 0) {
        char *p = g_find_program_in_path("canberra-gtk-play");
        have_canberra = p != NULL;
        g_free(p);
    }
    if (have_canberra) {
        char *argv[] = { "canberra-gtk-play", "-i", (char *)event_id, "-d", APP_TITLE, NULL };
        if (g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                          NULL, NULL, NULL, NULL))
            return;
    }
    gdk_display_beep(gdk_display_get_default());
}

void native_alert(GtkWindow *parent, GtkMessageType type, const char *text)
{
    GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", APP_TITLE);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", text);
    gtk_window_set_keep_above(GTK_WINDOW(d), TRUE);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}
