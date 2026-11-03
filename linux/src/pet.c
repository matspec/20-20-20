#include "pet.h"
#include "pixel.h"
#include "app.h"
#include <math.h>
#include <stdio.h>

struct Pet {
    Settings *s;
    Engine *e;
    const Skin *skin;
    GtkWidget *menu;
    GtkWidget *win;
    void (*toggle_pause)(void *ud);
    void *ud;

    int scale, w, h;
    int frame;
    int blink_for, far_for, bubble_for;
    gboolean alt, hover, pressed, moved;
    double press_x, press_y;
    guint anim, save_timer;
    Mood last_mood;
};

/* ---------- animation ---------- */

static gboolean step(gpointer ud)
{
    Pet *p = ud;
    p->frame++;
    /* Some window managers drop keep-above after other windows change state; re-assert ~once a second. */
    if (p->frame % 7 == 0 && !p->pressed) gtk_window_set_keep_above(GTK_WINDOW(p->win), TRUE);
    Mood mood = engine_mood(p->e);
    if (p->blink_for > 0) p->blink_for--;
    if (p->far_for > 0) p->far_for--;
    if (p->bubble_for > 0) p->bubble_for--;

    gboolean sleepy = mood == MOOD_SLEEPING || mood == MOOD_AWAY;
    if (!sleepy && p->blink_for == 0 && g_random_int_range(0, 100) < 4) p->blink_for = 2;
    /* The cat models good behaviour: it periodically gazes into the distance. */
    if (mood == MOOD_IDLE && p->far_for == 0 && g_random_int_range(0, 100) < 2) p->far_for = 12;

    int tail_every = (mood == MOOD_RESTLESS || mood == MOOD_HAPPY) ? 2 : sleepy ? 12 : 5;
    if (p->frame % tail_every == 0) p->alt = !p->alt;
    gtk_widget_queue_draw(p->win);
    return G_SOURCE_CONTINUE;
}

void pet_engine_changed(Pet *p)
{
    Mood m = engine_mood(p->e);
    if (m != p->last_mood) { p->last_mood = m; p->bubble_for = 25; }
    else if (m == MOOD_IDLE && p->e->remaining % 600 == 0) p->bubble_for = 20; /* every 10 min check-in */
}

static const char *frame_key(Pet *p)
{
    switch (engine_mood(p->e)) {
    case MOOD_SLEEPING: case MOOD_AWAY: return "sleep";
    case MOOD_HAPPY: return "happy";
    case MOOD_BREAK: return "wide";
    case MOOD_RESTLESS: return p->blink_for > 0 ? "blink" : (p->frame / 4 % 2 == 0 ? "idle" : "wide");
    default: return p->blink_for > 0 ? "blink" : p->far_for > 0 ? "far" : "idle";
    }
}

static void bubble_text(Pet *p, char *buf, size_t n)
{
    char t[32];
    switch (engine_mood(p->e)) {
    case MOOD_SLEEPING: snprintf(buf, n, "zzz... (paused)"); break;
    case MOOD_AWAY: snprintf(buf, n, "zzz..."); break;
    case MOOD_BREAK: snprintf(buf, n, "look far away!"); break;
    case MOOD_HAPPY: snprintf(buf, n, "good job!"); break;
    case MOOD_RESTLESS: snprintf(buf, n, "break soon... %s", engine_fmt(p->e->remaining, t, sizeof t)); break;
    default:
        if (p->bubble_for > 20 && p->frame < 60) snprintf(buf, n, "hi, i'm %s!", p->s->cat_name);
        else snprintf(buf, n, "next break: %s", engine_fmt(p->e->remaining, t, sizeof t));
    }
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer ud)
{
    Pet *p = ud;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    px_crisp(cr);

    int width = gtk_widget_get_allocated_width(w), height = gtk_widget_get_allocated_height(w);
    int sc = p->scale;
    int cat_w = p->skin->w * sc, cat_h = p->skin->h * sc;
    int cx = (width - cat_w) / 2, cy = height - cat_h;
    Mood mood = engine_mood(p->e);
    int bob = mood == MOOD_HAPPY ? (p->frame % 2) * sc : 0;
    px_sprite(cr, p->skin, skin_frame(p->skin, frame_key(p), p->alt), cx, cy - bob, sc);

    if (mood == MOOD_SLEEPING || mood == MOOD_AWAY) {
        int fs = MAX(1, sc / 2);
        int phase = p->frame / 3 % 4;
        for (int i = 0; i < 3; i++) {
            int a = 255 - i * 70 - phase * 20; if (a < 40) a = 40;
            Col c = PAL_ACCENT; c.a = a / 255.0;
            font_draw(cr, "z", cx + cat_w - 3 * sc + i * 3 * fs, cy - (2 + i * 4 + phase) * fs, fs, c);
        }
    }

    if (p->hover || p->bubble_for > 0 || mood == MOOD_BREAK) {
        int fs = MAX(1, sc / 2), px = MAX(1, sc / 2);
        char text[96];
        bubble_text(p, text, sizeof text);
        int tw = font_measure(text, fs);
        int bw = tw + 6 * px, bh = FONT_H * fs + 6 * px;
        int bx = MAX(0, MIN(width - bw, cx + cat_w / 2 - bw / 2));
        int by = cy - bh - 3 * px - bob;
        if (by < 0) by = 0;
        px_bubble(cr, bx, by, bw, bh, px, cx + cat_w / 2 - px);
        font_draw(cr, text, bx + 3 * px, by + 3 * px, fs, PAL_TEXT);
    }
    return TRUE;
}

/* ---------- mouse ---------- */

static gboolean on_press(GtkWidget *w, GdkEventButton *ev, gpointer ud)
{
    Pet *p = ud;
    (void)w;
    if (ev->button == 3) { gtk_menu_popup_at_pointer(GTK_MENU(p->menu), (GdkEvent *)ev); return TRUE; }
    if (ev->button != 1) return FALSE;
    if (ev->type == GDK_2BUTTON_PRESS) { p->pressed = FALSE; if (p->toggle_pause) p->toggle_pause(p->ud); return TRUE; }
    if (ev->type != GDK_BUTTON_PRESS) return TRUE;
    p->pressed = TRUE; p->moved = FALSE;
    p->press_x = ev->x_root; p->press_y = ev->y_root;
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *ev, gpointer ud)
{
    Pet *p = ud;
    if (!p->pressed || p->moved) return FALSE;
    if (fabs(ev->x_root - p->press_x) > 3 || fabs(ev->y_root - p->press_y) > 3) {
        p->moved = TRUE;
        /* Let the window manager do the drag: works on X11 and Wayland alike. */
        gtk_window_begin_move_drag(GTK_WINDOW(w), 1, (int)p->press_x, (int)p->press_y, ev->time);
    }
    return TRUE;
}

static gboolean on_release(GtkWidget *w, GdkEventButton *ev, gpointer ud)
{
    Pet *p = ud;
    (void)w;
    if (ev->button != 1 || !p->pressed) return FALSE;
    p->pressed = FALSE;
    if (!p->moved) p->bubble_for = p->bubble_for > 0 ? 0 : 25;
    return TRUE;
}

static gboolean save_position(gpointer ud)
{
    Pet *p = ud;
    p->save_timer = 0;
    int x, y;
    gtk_window_get_position(GTK_WINDOW(p->win), &x, &y);
    p->s->pet_x = x; p->s->pet_y = y; p->s->pet_placed = TRUE;
    settings_save(p->s);
    return G_SOURCE_REMOVE;
}

static gboolean on_configure(GtkWidget *w, GdkEventConfigure *ev, gpointer ud)
{
    Pet *p = ud;
    (void)w; (void)ev;
    if (p->moved) {
        /* A WM drag produces a stream of configure events; the pointer release never reaches us. */
        p->pressed = FALSE;
        if (p->save_timer) g_source_remove(p->save_timer);
        p->save_timer = g_timeout_add(400, save_position, p);
    }
    return FALSE;
}

static gboolean on_enter(GtkWidget *w, GdkEventCrossing *ev, gpointer ud)
{ (void)w; (void)ev; Pet *p = ud; p->hover = TRUE; gtk_widget_queue_draw(p->win); return FALSE; }
static gboolean on_leave(GtkWidget *w, GdkEventCrossing *ev, gpointer ud)
{ (void)w; (void)ev; Pet *p = ud; p->hover = FALSE; gtk_widget_queue_draw(p->win); return FALSE; }
static gboolean on_delete(GtkWidget *w, GdkEvent *ev, gpointer ud) { (void)w; (void)ev; (void)ud; return TRUE; }

/* ---------- placement ---------- */

static void initial_position(Pet *p, int *x, int *y)
{
    GdkDisplay *d = gdk_display_get_default();
    int n = gdk_display_get_n_monitors(d);
    if (p->s->pet_placed) {
        GdkRectangle r = { p->s->pet_x, p->s->pet_y, p->w, p->h };
        for (int i = 0; i < n; i++) {
            GdkRectangle wa;
            gdk_monitor_get_workarea(gdk_display_get_monitor(d, i), &wa);
            if (r.x >= wa.x && r.y >= wa.y && r.x + r.width <= wa.x + wa.width && r.y + r.height <= wa.y + wa.height) {
                *x = r.x; *y = r.y; return;
            }
        }
    }
    GdkMonitor *m = gdk_display_get_primary_monitor(d);
    if (!m && n > 0) m = gdk_display_get_monitor(d, 0);
    GdkRectangle wa = { 0, 0, 1280, 720 };
    if (m) gdk_monitor_get_workarea(m, &wa);
    *x = wa.x + wa.width - p->w - 24;
    *y = wa.y + wa.height - p->h;
}

Pet *pet_new(Settings *s, Engine *e, const Skin *skin, GtkWidget *menu, void (*toggle_pause)(void *), void *ud)
{
    Pet *p = g_new0(Pet, 1);
    p->s = s; p->e = e; p->skin = skin; p->menu = menu; p->toggle_pause = toggle_pause; p->ud = ud;
    p->scale = CLAMP(s->pet_scale, 2, 8);
    p->w = MAX(60, skin->w + 44) * p->scale;
    p->h = (skin->h + 16) * p->scale;
    p->last_mood = engine_mood(e);
    p->bubble_for = 30; /* say hi on launch */

    GtkWidget *w = p->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), s->cat_name);
    gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(w), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(w), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(w), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(w), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(w), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_stick(GTK_WINDOW(w));
    gtk_widget_set_app_paintable(w, TRUE);
    GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(w));
    if (visual) gtk_widget_set_visual(w, visual);
    gtk_widget_set_size_request(w, p->w, p->h);
    gtk_window_set_default_size(GTK_WINDOW(w), p->w, p->h);
    gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                             GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect(w, "draw", G_CALLBACK(on_draw), p);
    g_signal_connect(w, "button-press-event", G_CALLBACK(on_press), p);
    g_signal_connect(w, "button-release-event", G_CALLBACK(on_release), p);
    g_signal_connect(w, "motion-notify-event", G_CALLBACK(on_motion), p);
    g_signal_connect(w, "enter-notify-event", G_CALLBACK(on_enter), p);
    g_signal_connect(w, "leave-notify-event", G_CALLBACK(on_leave), p);
    g_signal_connect(w, "configure-event", G_CALLBACK(on_configure), p);
    g_signal_connect(w, "delete-event", G_CALLBACK(on_delete), p);

    int x, y;
    initial_position(p, &x, &y);
    gtk_window_move(GTK_WINDOW(w), x, y);
    gtk_widget_show_all(w);
    gtk_window_move(GTK_WINDOW(w), x, y);   /* some WMs ignore the pre-map position */

    p->anim = g_timeout_add(140, step, p);
    return p;
}

void pet_set_visible(Pet *p, gboolean visible)
{
    if (visible) gtk_widget_show(p->win); else gtk_widget_hide(p->win);
}

void pet_redraw(Pet *p) { gtk_widget_queue_draw(p->win); }

void pet_free(Pet *p)
{
    if (!p) return;
    if (p->anim) g_source_remove(p->anim);
    if (p->save_timer) { g_source_remove(p->save_timer); save_position(p); }
    gtk_widget_destroy(p->win);
    g_free(p);
}
