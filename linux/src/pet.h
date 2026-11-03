#ifndef PET_H
#define PET_H

#include <gtk/gtk.h>
#include "settings.h"
#include "engine.h"
#include "skin.h"

/* The always-on desktop buddy: undecorated ARGB window, keep-above, no focus, draggable. */
typedef struct Pet Pet;

Pet *pet_new(Settings *s, Engine *e, const Skin *skin, GtkWidget *menu, void (*toggle_pause)(void *ud), void *ud);
void pet_free(Pet *p);
void pet_engine_changed(Pet *p);
void pet_set_visible(Pet *p, gboolean visible);
void pet_redraw(Pet *p);

#endif
