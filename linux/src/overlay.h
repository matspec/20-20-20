#ifndef OVERLAY_H
#define OVERLAY_H

#include <gtk/gtk.h>
#include "settings.h"
#include "skin.h"

/* One break = one BreakSession, which opens an overlay on every monitor (or a single window
 * in non-full-screen mode) and closes them all together. */
typedef struct BreakSession BreakSession;

BreakSession *break_session_new(Settings *s, const Skin *skin, void (*done)(gboolean completed, void *ud), void *ud);
/* Ends the session (as skipped) if still running, then frees it. */
void break_session_free(BreakSession *b);

#endif
