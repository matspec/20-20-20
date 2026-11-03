#include "engine.h"
#include "native.h"
#include <gio/gio.h>
#include <stdio.h>

#define IDLE_RESET_SECONDS (5 * 60)
#define RESTLESS_SECONDS (2 * 60)
#define HAPPY_SECONDS 8

static void raise_changed(Engine *e) { if (e->on_changed) e->on_changed(e->ud); }

Mood engine_mood(const Engine *e)
{
    if (e->break_active) return MOOD_BREAK;
    if (e->settings->paused) return MOOD_SLEEPING;
    if (e->away) return MOOD_AWAY;
    if (e->happy_left > 0) return MOOD_HAPPY;
    return e->remaining <= RESTLESS_SECONDS ? MOOD_RESTLESS : MOOD_IDLE;
}

static void go_away(Engine *e) { e->away = TRUE; raise_changed(e); }
static void came_back(Engine *e) { e->away = FALSE; e->remaining = e->settings->work_minutes * 60; raise_changed(e); }

static gboolean on_tick(gpointer ud)
{
    Engine *e = ud;
    if (e->happy_left > 0) e->happy_left--;
    if (e->settings->paused || e->away || e->break_active) { raise_changed(e); return G_SOURCE_CONTINUE; }

    if (native_idle_seconds() >= IDLE_RESET_SECONDS) {
        e->remaining = e->settings->work_minutes * 60;
        raise_changed(e);
        return G_SOURCE_CONTINUE;
    }

    if (--e->remaining <= 0) { e->remaining = 0; engine_request_break(e); return G_SOURCE_CONTINUE; }
    raise_changed(e);
    return G_SOURCE_CONTINUE;
}

/* Signals carrying a single boolean: TRUE = going away (sleep / lock / screensaver on). */
static void on_bool_signal(GDBusConnection *c, const gchar *sender, const gchar *path, const gchar *iface,
                           const gchar *member, GVariant *params, gpointer ud)
{
    (void)c; (void)sender; (void)path; (void)iface; (void)member;
    gboolean b = FALSE;
    if (g_variant_is_of_type(params, G_VARIANT_TYPE("(b)"))) g_variant_get(params, "(b)", &b);
    if (b) go_away(ud); else came_back(ud);
}

static void on_lock(GDBusConnection *c, const gchar *s, const gchar *p, const gchar *i, const gchar *m, GVariant *v, gpointer ud)
{ (void)c; (void)s; (void)p; (void)i; (void)m; (void)v; go_away(ud); }
static void on_unlock(GDBusConnection *c, const gchar *s, const gchar *p, const gchar *i, const gchar *m, GVariant *v, gpointer ud)
{ (void)c; (void)s; (void)p; (void)i; (void)m; (void)v; came_back(ud); }

static void subscribe(Engine *e, GDBusConnection *bus, const char *iface, const char *member, GDBusSignalCallback cb)
{
    if (!bus) return;
    guint id = g_dbus_connection_signal_subscribe(bus, NULL, iface, member, NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, cb, e, NULL);
    g_array_append_val(e->subs, id);
    g_ptr_array_add(e->sub_bus, bus);
}

Engine *engine_new(Settings *s)
{
    Engine *e = g_new0(Engine, 1);
    e->settings = s;
    e->remaining = s->work_minutes * 60;
    e->subs = g_array_new(FALSE, FALSE, sizeof(guint));
    e->sub_bus = g_ptr_array_new();

    e->sys_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    e->ses_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    /* Suspend / resume */
    subscribe(e, e->sys_bus, "org.freedesktop.login1.Manager", "PrepareForSleep", on_bool_signal);
    /* Session lock / unlock (logind) */
    subscribe(e, e->sys_bus, "org.freedesktop.login1.Session", "Lock", on_lock);
    subscribe(e, e->sys_bus, "org.freedesktop.login1.Session", "Unlock", on_unlock);
    /* Screensaver / lock screen (GNOME, KDE, Cinnamon, MATE, XFCE all emit one of these) */
    subscribe(e, e->ses_bus, "org.gnome.ScreenSaver", "ActiveChanged", on_bool_signal);
    subscribe(e, e->ses_bus, "org.freedesktop.ScreenSaver", "ActiveChanged", on_bool_signal);
    subscribe(e, e->ses_bus, "org.cinnamon.ScreenSaver", "ActiveChanged", on_bool_signal);
    subscribe(e, e->ses_bus, "org.mate.ScreenSaver", "ActiveChanged", on_bool_signal);

    e->tick = g_timeout_add_seconds(1, on_tick, e);
    return e;
}

void engine_free(Engine *e)
{
    if (!e) return;
    if (e->tick) g_source_remove(e->tick);
    for (guint i = 0; i < e->subs->len; i++)
        g_dbus_connection_signal_unsubscribe(g_ptr_array_index(e->sub_bus, i), g_array_index(e->subs, guint, i));
    g_array_free(e->subs, TRUE);
    g_ptr_array_free(e->sub_bus, TRUE);
    g_clear_object(&e->sys_bus);
    g_clear_object(&e->ses_bus);
    g_free(e);
}

void engine_request_break(Engine *e)
{
    if (e->break_active) return;
    e->break_active = TRUE;
    raise_changed(e);
    if (e->on_break_requested) e->on_break_requested(e->ud);
}

void engine_complete_break(Engine *e, gboolean finished)
{
    e->break_active = FALSE;
    e->remaining = e->settings->work_minutes * 60;
    e->happy_left = finished ? HAPPY_SECONDS : 0;
    raise_changed(e);
}

void engine_reset(Engine *e) { e->remaining = e->settings->work_minutes * 60; raise_changed(e); }
void engine_toggle_pause(Engine *e) { e->settings->paused = !e->settings->paused; settings_save(e->settings); raise_changed(e); }
void engine_interval_changed(Engine *e) { e->remaining = MIN(e->remaining, e->settings->work_minutes * 60); raise_changed(e); }

const char *engine_fmt(int secs, char *buf, size_t n)
{
    if (secs >= 60) snprintf(buf, n, "%d min", (secs + 59) / 60);
    else snprintf(buf, n, "%d sec", secs);
    return buf;
}

const char *engine_status(const Engine *e, char *buf, size_t n)
{
    char t[32];
    switch (engine_mood(e)) {
    case MOOD_SLEEPING: snprintf(buf, n, "Paused"); break;
    case MOOD_AWAY: snprintf(buf, n, "Away"); break;
    case MOOD_BREAK: snprintf(buf, n, "Break time"); break;
    default: snprintf(buf, n, "Next break in %s", engine_fmt(e->remaining, t, sizeof t)); break;
    }
    return buf;
}
