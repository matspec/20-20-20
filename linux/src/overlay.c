#include "overlay.h"
#include "pixel.h"
#include "native.h"
#include "app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    BreakSession *s;
    GtkWidget *win;
    gboolean primary, windowed;
    GdkRectangle done_rect, skip_rect;
} Overlay;

struct BreakSession {
    Settings *s;
    const Skin *skin;
    void (*done)(gboolean, void *);
    void *ud;
    GPtrArray *overlays;
    guint second, anim;
    int seconds_left, frame;
    gboolean alt, closed;
};

static gboolean finished(const BreakSession *b) { return b->seconds_left <= 0; }

/* Tears everything down, frees the session, then reports the result (callback runs last so it
 * may safely drop its own pointer; it must not call break_session_free afterwards). */
static void end(BreakSession *b, gboolean completed)
{
    if (b->closed) return;
    b->closed = TRUE;
    if (b->second) g_source_remove(b->second);
    if (b->anim) g_source_remove(b->anim);
    for (guint i = 0; i < b->overlays->len; i++) {
        Overlay *o = g_ptr_array_index(b->overlays, i);
        gtk_widget_destroy(o->win);
    }
    g_ptr_array_free(b->overlays, TRUE);
    void (*done)(gboolean, void *) = b->done;
    void *ud = b->ud;
    g_free(b);
    if (done) done(completed, ud);
}

void break_session_free(BreakSession *b) { if (b) end(b, FALSE); }

static void redraw(BreakSession *b)
{
    for (guint i = 0; i < b->overlays->len; i++) {
        Overlay *o = g_ptr_array_index(b->overlays, i);
        if (b->frame % 7 == 0) gtk_window_set_keep_above(GTK_WINDOW(o->win), TRUE);
        gtk_widget_queue_draw(o->win);
    }
}

static gboolean on_second(gpointer ud)
{
    BreakSession *b = ud;
    if (b->seconds_left <= 0) return G_SOURCE_CONTINUE;
    b->seconds_left--;
    if (b->seconds_left == 0) {
        b->second = 0;   /* returning G_SOURCE_REMOVE destroys the source */
        if (b->s->play_sound) native_play_sound("complete");
        redraw(b);
        return G_SOURCE_REMOVE;
    }
    redraw(b);
    return G_SOURCE_CONTINUE;
}

static gboolean on_anim(gpointer ud)
{
    BreakSession *b = ud;
    b->frame++;
    if (b->frame % 3 == 0) b->alt = !b->alt;
    redraw(b);
    return G_SOURCE_CONTINUE;
}

/* ---------- input ---------- */

static gboolean on_key(GtkWidget *w, GdkEventKey *ev, gpointer ud)
{
    Overlay *o = ud; (void)w;
    switch (ev->keyval) {
    case GDK_KEY_Escape: end(o->s, FALSE); return TRUE;
    case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_space:
        if (finished(o->s)) end(o->s, TRUE);
        return TRUE;
    }
    return FALSE;
}

static gboolean in_rect(const GdkRectangle *r, double x, double y)
{
    return x >= r->x && y >= r->y && x < r->x + r->width && y < r->y + r->height;
}

static gboolean on_release(GtkWidget *w, GdkEventButton *ev, gpointer ud)
{
    Overlay *o = ud; (void)w;
    if (!o->primary || ev->button != 1) return FALSE;
    if (finished(o->s) && in_rect(&o->done_rect, ev->x, ev->y)) end(o->s, TRUE);
    else if (in_rect(&o->skip_rect, ev->x, ev->y)) end(o->s, FALSE);
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer ud)
{
    Overlay *o = ud;
    gboolean hot = o->primary && (in_rect(&o->skip_rect, ev->x, ev->y) || (finished(o->s) && in_rect(&o->done_rect, ev->x, ev->y)));
    GdkWindow *gw = gtk_widget_get_window(w);
    if (!gw) return FALSE;
    if (hot) {
        GdkCursor *c = gdk_cursor_new_from_name(gdk_window_get_display(gw), "pointer");
        gdk_window_set_cursor(gw, c);
        if (c) g_object_unref(c);
    } else {
        gdk_window_set_cursor(gw, NULL);
    }
    return FALSE;
}

static gboolean on_delete(GtkWidget *w, GdkEvent *ev, gpointer ud) { (void)w; (void)ev; Overlay *o = ud; end(o->s, FALSE); return TRUE; }

/* ---------- painting ---------- */

static void draw_button(cairo_t *cr, GdkRectangle r, const char *text, int fs, int px, Col fill, Col fg)
{
    px_fill(cr, r.x + px, r.y, r.width - 2 * px, r.height, fill);
    px_fill(cr, r.x, r.y + px, r.width, r.height - 2 * px, fill);
    int tw = font_measure(text, fs);
    font_draw(cr, text, r.x + (r.width - tw) / 2, r.y + (r.height - FONT_H * fs) / 2, fs, fg);
}

static void paint_primary(Overlay *o, cairo_t *cr, int width, int height)
{
    BreakSession *b = o->s;
    /* Scale everything off the shorter edge so it looks the same on a 1080p or a 4K panel. */
    int unit = MAX(2, MIN(width, height) / 160);   /* base pixel size */
    int cat_scale = unit * 3;
    int fs_big = unit * 4, fs_mid = unit, fs_small = MAX(1, unit * 3 / 4);
    gboolean done = finished(b);

    /* Cat, gazing into the distance (or beaming when done). */
    const Skin *skin = b->skin;
    cat_scale = MAX(2, cat_scale * 16 / MAX(skin->w, skin->h));
    const char *key = done ? "happy" : (b->frame / 20 % 5 == 0 ? "blink" : "far");
    int cat_w = skin->w * cat_scale, cat_h = skin->h * cat_scale;
    int cx = (width - cat_w) / 2;
    int cy = height / 2 - cat_h - unit * 6 + (done ? (b->frame % 2) * unit : 0);
    px_sprite(cr, skin, skin_frame(skin, key, b->alt), cx, cy, cat_scale);

    /* Speech bubble. */
    const char *say = done ? "that's it! back to work?" : "look at something far away...";
    int px = unit;
    int tw = font_measure(say, fs_mid);
    int bw = tw + 6 * px, bh = FONT_H * fs_mid + 6 * px;
    int bx = (width - bw) / 2, by = cy - bh - 4 * px;
    px_bubble(cr, bx, by, bw, bh, px, cx + cat_w / 2 - px);
    font_draw(cr, say, bx + 3 * px, by + 3 * px, fs_mid, PAL_TEXT);

    /* Big countdown. */
    char big[16];
    if (done) strcpy(big, "DONE"); else snprintf(big, sizeof big, "%02d", b->seconds_left);
    int big_w = font_measure(big, fs_big);
    int big_y = cy + cat_h + unit * 6;
    font_draw(cr, big, (width - big_w) / 2, big_y, fs_big, PAL_ACCENT);

    /* Progress bar (pixel segments). */
    int segs = 20, seg_w = unit * 3, gap = unit;
    int bar_w = segs * seg_w + (segs - 1) * gap;
    int bar_x = (width - bar_w) / 2, bar_y = big_y + FONT_H * fs_big + unit * 4;
    int lit = done ? segs : (int)lround(segs * (1.0 - (double)b->seconds_left / b->s->break_seconds));
    for (int i = 0; i < segs; i++)
        px_fill(cr, bar_x + i * (seg_w + gap), bar_y, seg_w, unit * 2, i < lit ? PAL_ACCENT : PAL_GRID);

    /* Caption. */
    char cap[96];
    if (done) { char *up = g_ascii_strup(b->s->cat_name, -1); snprintf(cap, sizeof cap, "%s APPROVES", up); g_free(up); }
    else strcpy(cap, "20 FEET  /  20 SECONDS  /  EVERY 20 MINUTES");
    int cw = font_measure(cap, fs_small);
    font_draw(cr, cap, (width - cw) / 2, bar_y + unit * 6, fs_small, PAL_TEXT_DIM);

    /* Buttons. */
    int btn_h = FONT_H * fs_mid + 6 * unit, btn_y = height - btn_h - unit * 8;
    int done_w = font_measure("DONE", fs_mid) + 10 * unit, skip_w = font_measure("SKIP", fs_mid) + 10 * unit;
    int total_w = done_w + skip_w + unit * 4;
    o->done_rect = (GdkRectangle){ (width - total_w) / 2, btn_y, done_w, btn_h };
    o->skip_rect = (GdkRectangle){ o->done_rect.x + o->done_rect.width + unit * 4, btn_y, skip_w, btn_h };
    draw_button(cr, o->done_rect, "DONE", fs_mid, unit, done ? PAL_ACCENT : PAL_GRID, done ? PAL_BG : PAL_TEXT_DIM);
    draw_button(cr, o->skip_rect, "SKIP", fs_mid, unit, PAL_BUBBLE, PAL_TEXT_DIM);

    const char *hint = done ? "ENTER TO CONTINUE" : "ESC TO SKIP";
    int hw = font_measure(hint, fs_small);
    font_draw(cr, hint, (width - hw) / 2, height - unit * 5, fs_small, PAL_TEXT_DIM);
}

static void paint_secondary(Overlay *o, cairo_t *cr, int width, int height)
{
    BreakSession *b = o->s;
    int unit = MAX(2, MIN(width, height) / 160);
    int fs_big = unit * 3, fs_small = unit;
    char big[16];
    if (finished(b)) strcpy(big, "DONE"); else snprintf(big, sizeof big, "%02d", b->seconds_left);
    int bw = font_measure(big, fs_big);
    font_draw(cr, big, (width - bw) / 2, height / 2 - FONT_H * fs_big, fs_big, PAL_ACCENT);
    const char *s = finished(b) ? "NICE" : "LOOK AWAY FROM THE SCREEN";
    int sw = font_measure(s, fs_small);
    font_draw(cr, s, (width - sw) / 2, height / 2 + unit * 4, fs_small, PAL_TEXT_DIM);
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer ud)
{
    Overlay *o = ud;
    int width = gtk_widget_get_allocated_width(w), height = gtk_widget_get_allocated_height(w);
    px_crisp(cr);
    px_fill(cr, 0, 0, width, height, PAL_BG);
    px_grid(cr, 0, 0, width, height, MAX(24, height / 24));

    if (o->primary) paint_primary(o, cr, width, height); else paint_secondary(o, cr, width, height);

    px_scanlines(cr, 0, 0, width, height, 60);
    if (o->windowed) {
        cairo_set_source_rgb(cr, PAL_ACCENT_DIM.r, PAL_ACCENT_DIM.g, PAL_ACCENT_DIM.b);
        cairo_set_line_width(cr, 2);
        cairo_rectangle(cr, 2, 2, width - 4, height - 4);
        cairo_stroke(cr);
    }
    return TRUE;
}

/* ---------- windows ---------- */

static Overlay *overlay_new(BreakSession *b, gboolean primary, gboolean windowed)
{
    Overlay *o = g_new0(Overlay, 1);
    o->s = b; o->primary = primary; o->windowed = windowed;
    GtkWidget *w = o->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), APP_TITLE);
    gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(w), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(w), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(w), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(w), primary);
    gtk_window_set_focus_on_map(GTK_WINDOW(w), primary);
    gtk_window_stick(GTK_WINDOW(w));
    gtk_widget_set_app_paintable(w, TRUE);
    gtk_widget_add_events(w, GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(w, "draw", G_CALLBACK(on_draw), o);
    g_signal_connect(w, "key-press-event", G_CALLBACK(on_key), o);
    g_signal_connect(w, "button-release-event", G_CALLBACK(on_release), o);
    g_signal_connect(w, "motion-notify-event", G_CALLBACK(on_motion), o);
    g_signal_connect(w, "delete-event", G_CALLBACK(on_delete), o);
    return o;
}

BreakSession *break_session_new(Settings *s, const Skin *skin, void (*done)(gboolean, void *), void *ud)
{
    BreakSession *b = g_new0(BreakSession, 1);
    b->s = s; b->skin = skin; b->done = done; b->ud = ud;
    b->seconds_left = s->break_seconds;
    b->overlays = g_ptr_array_new_with_free_func(g_free);

    GdkDisplay *d = gdk_display_get_default();
    GdkScreen *screen = gdk_screen_get_default();
    int n = gdk_display_get_n_monitors(d);
    GdkMonitor *prim = gdk_display_get_primary_monitor(d);
    if (!prim && n > 0) prim = gdk_display_get_monitor(d, 0);

    if (s->full_screen) {
        for (int i = 0; i < n; i++) {
            GdkMonitor *m = gdk_display_get_monitor(d, i);
            Overlay *o = overlay_new(b, m == prim, FALSE);
            GdkRectangle g; gdk_monitor_get_geometry(m, &g);
            gtk_window_set_default_size(GTK_WINDOW(o->win), g.width, g.height);
            gtk_window_move(GTK_WINDOW(o->win), g.x, g.y);
            gtk_window_fullscreen_on_monitor(GTK_WINDOW(o->win), screen, i);
            if (!o->primary) gtk_widget_set_opacity(o->win, 0.92);
            g_ptr_array_add(b->overlays, o);
        }
    } else {
        GdkRectangle wa = { 0, 0, 1280, 720 };
        if (prim) gdk_monitor_get_workarea(prim, &wa);
        int w = MIN(560, wa.width - 40), h = MIN(380, wa.height - 40);
        Overlay *o = overlay_new(b, TRUE, TRUE);
        gtk_window_set_type_hint(GTK_WINDOW(o->win), GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_default_size(GTK_WINDOW(o->win), w, h);
        gtk_window_set_resizable(GTK_WINDOW(o->win), FALSE);
        gtk_window_move(GTK_WINDOW(o->win), wa.x + (wa.width - w) / 2, wa.y + (wa.height - h) / 2);
        g_ptr_array_add(b->overlays, o);
    }

    /* Show secondaries first so the primary ends up on top with focus. */
    for (guint i = 0; i < b->overlays->len; i++) {
        Overlay *o = g_ptr_array_index(b->overlays, i);
        if (!o->primary) gtk_widget_show_all(o->win);
    }
    for (guint i = 0; i < b->overlays->len; i++) {
        Overlay *o = g_ptr_array_index(b->overlays, i);
        if (o->primary) { gtk_widget_show_all(o->win); gtk_window_present(GTK_WINDOW(o->win)); }
    }

    if (s->play_sound) native_play_sound("message");
    b->second = g_timeout_add_seconds(1, on_second, b);
    b->anim = g_timeout_add(140, on_anim, b);
    return b;
}
