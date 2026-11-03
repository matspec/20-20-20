#ifndef ENGINE_H
#define ENGINE_H

#include <gio/gio.h>
#include "settings.h"

typedef enum { MOOD_IDLE, MOOD_RESTLESS, MOOD_BREAK, MOOD_HAPPY, MOOD_SLEEPING, MOOD_AWAY } Mood;

/* Owns the countdown and the "what is the cat feeling" state. UI layers subscribe via on_changed. */
typedef struct {
    Settings *settings;
    int remaining;
    gboolean break_active;

    void (*on_changed)(void *ud);
    void (*on_break_requested)(void *ud);
    void *ud;

    /* private */
    int happy_left;
    gboolean away;
    guint tick;
    GDBusConnection *sys_bus, *ses_bus;
    GArray *subs;   /* guint subscription ids, paired with bus in sub_bus */
    GPtrArray *sub_bus;
} Engine;

Engine *engine_new(Settings *s);
void engine_free(Engine *e);

Mood engine_mood(const Engine *e);
void engine_request_break(Engine *e);
void engine_complete_break(Engine *e, gboolean finished);
void engine_reset(Engine *e);
void engine_toggle_pause(Engine *e);
void engine_interval_changed(Engine *e);

/* Writes "Paused" / "Away" / "Break time" / "Next break in N min" into buf. */
const char *engine_status(const Engine *e, char *buf, size_t n);
const char *engine_fmt(int secs, char *buf, size_t n);

#endif
