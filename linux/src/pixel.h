#ifndef PIXEL_H
#define PIXEL_H

#include <gtk/gtk.h>
#include "skin.h"

/* UI palette, a tiny 3x5 pixel font and a nearest-neighbour sprite renderer (cairo). */

extern const Col PAL_BG, PAL_GRID, PAL_ACCENT, PAL_ACCENT_DIM, PAL_TEXT, PAL_TEXT_DIM, PAL_BUBBLE, PAL_BUBBLE_BORDER;

#define FONT_W 3
#define FONT_H 5

int  font_measure(const char *text, int scale);
void font_draw(cairo_t *cr, const char *text, int x, int y, int scale, Col c);

void px_crisp(cairo_t *cr);
void px_fill(cairo_t *cr, int x, int y, int w, int h, Col c);
void px_sprite(cairo_t *cr, const Skin *skin, char **rows, int x, int y, int scale);
void px_bubble(cairo_t *cr, int x, int y, int w, int h, int px, int tail_x);
void px_scanlines(cairo_t *cr, int x, int y, int w, int h, int alpha);
void px_grid(cairo_t *cr, int x, int y, int w, int h, int step);

/* Fits a sprite into a square image of the given pixel size. Caller owns the result. */
GdkPixbuf *px_pixbuf(const Skin *skin, char **rows, int size);

#endif
