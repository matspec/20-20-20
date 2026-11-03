#ifndef NATIVE_H
#define NATIVE_H

#include <gtk/gtk.h>

/* Seconds since the last keyboard/mouse input. X11: XScreenSaver extension (if built with it).
 * Wayland/GNOME: org.gnome.Mutter.IdleMonitor over D-Bus. Otherwise 0 (idle reset disabled). */
int native_idle_seconds(void);

/* Plays a freedesktop sound theme event (e.g. "message", "complete") via canberra-gtk-play
 * if available, otherwise the display bell. */
void native_play_sound(const char *event_id);

void native_alert(GtkWindow *parent, GtkMessageType type, const char *text);

#endif
