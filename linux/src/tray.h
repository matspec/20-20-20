#ifndef TRAY_H
#define TRAY_H

#include <gtk/gtk.h>

/* Glues everything together: engine (logic), tray icon + menu, desktop pet, break overlays. */
typedef struct Tray Tray;

Tray *tray_new(GtkApplication *app);
void tray_free(Tray *t);

/* Writes app icon PNGs (<dir>/<size>.png) for the .desktop file / AppImage. */
gboolean tray_write_icons(const char *dir);

#endif
