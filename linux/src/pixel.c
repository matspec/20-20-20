#include "pixel.h"
#include <string.h>

#define RGB(r, g, b) { (r) / 255.0, (g) / 255.0, (b) / 255.0, 1.0 }
const Col PAL_BG = RGB(11, 15, 20);
const Col PAL_GRID = RGB(22, 30, 40);
const Col PAL_ACCENT = RGB(94, 240, 160);
const Col PAL_ACCENT_DIM = RGB(40, 120, 85);
const Col PAL_TEXT = RGB(225, 232, 240);
const Col PAL_TEXT_DIM = RGB(120, 135, 150);
const Col PAL_BUBBLE = RGB(28, 36, 48);
const Col PAL_BUBBLE_BORDER = RGB(94, 240, 160);

typedef struct { char ch; const char *rows[FONT_H]; } Glyph;
static const Glyph GLYPHS[] = {
    {'A', {"010","101","111","101","101"}}, {'B', {"110","101","110","101","110"}},
    {'C', {"011","100","100","100","011"}}, {'D', {"110","101","101","101","110"}},
    {'E', {"111","100","110","100","111"}}, {'F', {"111","100","110","100","100"}},
    {'G', {"011","100","101","101","011"}}, {'H', {"101","101","111","101","101"}},
    {'I', {"111","010","010","010","111"}}, {'J', {"001","001","001","101","010"}},
    {'K', {"101","101","110","101","101"}}, {'L', {"100","100","100","100","111"}},
    {'M', {"101","111","111","101","101"}}, {'N', {"110","101","101","101","101"}},
    {'O', {"010","101","101","101","010"}}, {'P', {"110","101","110","100","100"}},
    {'Q', {"010","101","101","011","001"}}, {'R', {"110","101","110","101","101"}},
    {'S', {"011","100","010","001","110"}}, {'T', {"111","010","010","010","010"}},
    {'U', {"101","101","101","101","111"}}, {'V', {"101","101","101","101","010"}},
    {'W', {"101","101","111","111","101"}}, {'X', {"101","101","010","101","101"}},
    {'Y', {"101","101","010","010","010"}}, {'Z', {"111","001","010","100","111"}},
    {'0', {"111","101","101","101","111"}}, {'1', {"010","110","010","010","111"}},
    {'2', {"111","001","111","100","111"}}, {'3', {"111","001","111","001","111"}},
    {'4', {"101","101","111","001","001"}}, {'5', {"111","100","111","001","111"}},
    {'6', {"111","100","111","101","111"}}, {'7', {"111","001","001","001","001"}},
    {'8', {"111","101","111","101","111"}}, {'9', {"111","101","111","001","111"}},
    {':', {"000","010","000","010","000"}}, {'!', {"010","010","010","000","010"}},
    {'.', {"000","000","000","000","010"}}, {'-', {"000","000","111","000","000"}},
    {'\'',{"010","010","000","000","000"}}, {',', {"000","000","000","010","100"}},
    {'?', {"111","001","011","000","010"}}, {'/', {"001","001","010","100","100"}},
    {'(', {"010","100","100","100","010"}}, {')', {"010","001","001","001","010"}},
    {'%', {"101","001","010","100","101"}}, {'~', {"000","010","101","000","000"}},
    {' ', {"000","000","000","000","000"}},
};

static const Glyph *glyph(char c)
{
    static const Glyph *table[128];
    static gboolean init;
    if (!init) {
        for (size_t i = 0; i < G_N_ELEMENTS(GLYPHS); i++) table[(unsigned char)GLYPHS[i].ch] = &GLYPHS[i];
        init = TRUE;
    }
    unsigned char u = (unsigned char)g_ascii_toupper(c);
    return u < 128 ? table[u] : NULL;
}

int font_measure(const char *text, int scale)
{
    int n = (int)strlen(text);
    return n == 0 ? 0 : (n * (FONT_W + 1) - 1) * scale;
}

void px_fill(cairo_t *cr, int x, int y, int w, int h, Col c)
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
}

void font_draw(cairo_t *cr, const char *text, int x, int y, int scale, Col c)
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
    for (const char *p = text; *p; p++) {
        const Glyph *g = glyph(*p);
        if (g)
            for (int r = 0; r < FONT_H; r++)
                for (int col = 0; col < FONT_W; col++)
                    if (g->rows[r][col] == '1') cairo_rectangle(cr, x + col * scale, y + r * scale, scale, scale);
        x += (FONT_W + 1) * scale;
    }
    cairo_fill(cr);
}

void px_crisp(cairo_t *cr)
{
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
}

/* Draws one frame directly as filled rectangles, merging horizontal runs of the same colour. */
void px_sprite(cairo_t *cr, const Skin *skin, char **rows, int x, int y, int scale)
{
    for (int r = 0; rows[r]; r++) {
        const char *row = rows[r];
        int n = (int)strlen(row), c = 0;
        while (c < n) {
            Col col;
            if (!skin_color(skin, row[c], &col)) { c++; continue; }
            int run = 1;
            while (c + run < n && row[c + run] == row[c]) run++;
            px_fill(cr, x + c * scale, y + r * scale, run * scale, scale, col);
            c += run;
        }
    }
}

/* Speech bubble with a little pixel tail pointing down to (tail_x, bottom). */
void px_bubble(cairo_t *cr, int x, int y, int w, int h, int p, int tail_x)
{
    int bottom = y + h;
    px_fill(cr, x, y + p, w, h - 2 * p, PAL_BUBBLE_BORDER);
    px_fill(cr, x + p, y, w - 2 * p, h, PAL_BUBBLE_BORDER);
    px_fill(cr, x + p, y + p, w - 2 * p, h - 2 * p, PAL_BUBBLE);
    px_fill(cr, tail_x - p, bottom, 3 * p, p, PAL_BUBBLE_BORDER);
    px_fill(cr, tail_x, bottom + p, p, p, PAL_BUBBLE_BORDER);
    px_fill(cr, tail_x, bottom - p, p, p, PAL_BUBBLE);
}

void px_scanlines(cairo_t *cr, int x, int y, int w, int h, int alpha)
{
    cairo_set_source_rgba(cr, 0, 0, 0, alpha / 255.0);
    for (int yy = y; yy < y + h; yy += 3) cairo_rectangle(cr, x, yy, w, 1);
    cairo_fill(cr);
}

void px_grid(cairo_t *cr, int x, int y, int w, int h, int step)
{
    cairo_set_source_rgba(cr, PAL_GRID.r, PAL_GRID.g, PAL_GRID.b, 1);
    for (int xx = x; xx < x + w; xx += step) cairo_rectangle(cr, xx, y, 1, h);
    for (int yy = y; yy < y + h; yy += step) cairo_rectangle(cr, x, yy, w, 1);
    cairo_fill(cr);
}

GdkPixbuf *px_pixbuf(const Skin *skin, char **rows, int size)
{
    int scale = MAX(1, size / MAX(skin->w, skin->h));
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surf);
    px_crisp(cr);
    int sw = skin->w * scale, sh = skin->h * scale;
    px_sprite(cr, skin, rows, (size - sw) / 2, (size - sh) / 2, scale);
    cairo_destroy(cr);
    cairo_surface_flush(surf);
    GdkPixbuf *pb = gdk_pixbuf_get_from_surface(surf, 0, 0, size, size);
    cairo_surface_destroy(surf);
    return pb;
}
