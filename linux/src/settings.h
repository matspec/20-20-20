#ifndef SETTINGS_H
#define SETTINGS_H

#include <glib.h>

typedef struct {
    int work_minutes;
    int break_seconds;
    gboolean full_screen;
    gboolean play_sound;
    gboolean paused;
    gboolean show_pet;
    char cat_name[64];
    char skin_name[128];
    int pet_scale;
    /* Window position (top-left, root coordinates). Can be negative on multi-monitor setups,
       so a separate flag records whether the user has placed the pet. */
    int pet_x, pet_y;
    gboolean pet_placed;
} Settings;

/* $XDG_CONFIG_HOME/TwentyTwentyTwenty (usually ~/.config/TwentyTwentyTwenty) */
const char *settings_dir(void);
gboolean settings_exists(void);
void settings_load(Settings *s);
void settings_save(const Settings *s);

/* Autostart via ~/.config/autostart/*.desktop (freedesktop standard, no root). */
gboolean settings_startup_enabled(void);
void settings_set_startup(gboolean enabled);

#endif
