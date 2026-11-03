#include "app.h"
#include "tray.h"
#include "native.h"
#include <stdio.h>

static Tray *tray;
static char *make_icons_dir;
static gboolean minimized;

static void on_activate(GtkApplication *app, gpointer ud)
{
    (void)ud;
    if (tray) {
        /* A second instance was launched: GApplication forwarded its activate to us. */
        native_alert(NULL, GTK_MESSAGE_INFO, APP_TITLE " is already running. Look for the cat in the system tray.");
        return;
    }
    g_application_hold(G_APPLICATION(app));   /* no windows at first, keep running */
    tray = tray_new(app);
}

static void on_shutdown(GApplication *app, gpointer ud)
{
    (void)app; (void)ud;
    tray_free(tray);
    tray = NULL;
}

static gint on_local_options(GApplication *app, GVariantDict *opts, gpointer ud)
{
    (void)app; (void)opts; (void)ud;
    if (make_icons_dir) {
        gboolean ok = tray_write_icons(make_icons_dir);
        if (!ok) fprintf(stderr, "failed to write icons to %s\n", make_icons_dir);
        return ok ? 0 : 1;
    }
    return -1;   /* continue normal startup */
}

int main(int argc, char **argv)
{
    static const GOptionEntry entries[] = {
        { "minimized", 0, 0, G_OPTION_ARG_NONE, &minimized, "Start quietly (used by autostart)", NULL },
        { "make-icons", 0, 0, G_OPTION_ARG_FILENAME, &make_icons_dir, "Write app icon PNGs into DIR and exit", "DIR" },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
    g_set_prgname(APP_EXE);
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_FLAGS_NONE);
    g_application_add_main_option_entries(G_APPLICATION(app), entries);
    g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);
    int rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return rc;
}
