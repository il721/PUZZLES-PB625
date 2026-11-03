/* Front end for Simon Tatham's puzzles on InkView: implements the engine's drawing API with the
 * screen primitives (FillArea, DrawLine, DrawTextRect, ...), plus the platform hooks the engine needs.
 * One puzzle is open at a time, so the state is a single static struct. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tat.h"

struct blitter {
    int w, h;          /* requested size */
    int x, y;          /* screen position of the saved top left corner */
    int cx, cy;        /* saved part (clipped to the screen) */
    ibitmap *bm;
};

#define MAXFONTS 24
struct frontend {
    int ox, oy;                       /* screen position of the puzzle's (0,0) */
    int ncol;
    int pal[64];                      /* engine colour index -> 0xGGGGGG */
    int thick;                        /* 1: draw 1-px lines twice as wide */
    int hold;                         /* 1: no screen refresh from end_draw */
    int ux0, uy0, ux1, uy1;           /* dirty rectangle (screen) of the redraw in progress */
    int timer_on;
    void (*status_cb)(const char *);
    struct { int size, fixed; ifont *f; } fonts[MAXFONTS];
    int nfonts;
};
static struct frontend FE;

/* ---- platform hooks the engine needs ---- */

void fatal(const char *fmt, ...)
{
    va_list ap;
    FILE *f = fopen(TAT_DIR "/tatham.log", "a");
    if (f) {
        fprintf(f, "fatal error: ");
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fprintf(f, "\n");
        fclose(f);
    }
    exit(2);
}

void frontend_default_colour(frontend *fe, float *out)
{
    (void)fe;
    out[0] = out[1] = out[2] = 0.92f;      /* the engine derives its shades from this; mapped to white below */
}
void deactivate_timer(frontend *fe) { fe->timer_on = 0; }
void activate_timer(frontend *fe) { fe->timer_on = 1; }

void get_random_seed(void **randseed, int *randseedsize)
{
    static unsigned long counter;
    unsigned long *p = snew(unsigned long);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *p = (unsigned long)ts.tv_sec * 1000003UL + (unsigned long)ts.tv_nsec + 7919UL * counter++ + (unsigned long)getpid();
    *randseed = p;
    *randseedsize = sizeof(*p);
}

void document_add_puzzle(document *doc, const game *g, game_params *par, game_ui *ui, game_state *st, game_state *st2)
{
    (void)doc; (void)g; (void)par; (void)ui; (void)st; (void)st2;     /* printing is not supported */
}

/* ---- colours ---- */

static int gray_of(float r, float g, float b)
{
    float l = 0.30f * r + 0.59f * g + 0.11f * b;
    int v;
    if (l >= 0.86f) return 0xffffff;                  /* paper and highlights: pure white, no dither noise */
    v = (int)(l * 255.0f + 0.5f);
    v = (v + 8) / 17 * 17;                            /* the panel has 16 gray levels */
    if (v > 255) v = 255;
    return v * 0x010101;
}

void fe_load_palette(midend *me)
{
    int n, i;
    float *c = midend_colours(me, &n);
    FE.ncol = n > 64 ? 64 : n;
    for (i = 0; i < FE.ncol; i++) FE.pal[i] = gray_of(c[3 * i], c[3 * i + 1], c[3 * i + 2]);
    sfree(c);
}

static int col(int c)
{
    if (c < 0 || c >= FE.ncol) return 0;
    return FE.pal[c];
}

/* ---- fonts ---- */

static ifont *get_font(int fixed, int size)
{
    int i;
    ifont *f;
    if (size < 8) size = 8;
    for (i = 0; i < FE.nfonts; i++)
        if (FE.fonts[i].size == size && FE.fonts[i].fixed == fixed) return FE.fonts[i].f;
    if (FE.nfonts == MAXFONTS) {                      /* drop the oldest */
        if (FE.fonts[0].f) CloseFont(FE.fonts[0].f);
        memmove(&FE.fonts[0], &FE.fonts[1], (MAXFONTS - 1) * sizeof FE.fonts[0]);
        FE.nfonts--;
    }
    f = fixed ? OpenFont("LiberationMono-Bold", size, 1) : OpenFont("LiberationSans-Bold", size, 1);
    if (!f) f = OpenFont("LiberationSans-Bold", size, 1);
    if (!f) f = OpenFont("LiberationSans", size, 1);
    FE.fonts[FE.nfonts].size = size;
    FE.fonts[FE.nfonts].fixed = fixed;
    FE.fonts[FE.nfonts].f = f;
    FE.nfonts++;
    return f;
}

void fe_init(void) { memset(&FE, 0, sizeof FE); }

void fe_free(void)
{
    int i;
    for (i = 0; i < FE.nfonts; i++)
        if (FE.fonts[i].f) CloseFont(FE.fonts[i].f);
    FE.nfonts = 0;
}

/* ---- drawing API ---- */

static void sx_clip_reset(void) { SetClip(0, 0, ScreenWidth(), ScreenHeight()); }

static void d_rect(drawing *dr, int x, int y, int w, int h, int c)
{
    (void)dr;
    if (w <= 0 || h <= 0) return;
    FillArea(FE.ox + x, FE.oy + y, w, h, col(c));
}

static int iabs(int v) { return v < 0 ? -v : v; }

static void d_line(drawing *dr, int x1, int y1, int x2, int y2, int c)
{
    int color = col(c), dx, dy, th;
    (void)dr;
    x1 += FE.ox; x2 += FE.ox; y1 += FE.oy; y2 += FE.oy;
    dx = iabs(x2 - x1); dy = iabs(y2 - y1);
    th = FE.thick ? 2 : 1;                            /* two pixels wide: a second line next to the first */
    if (x1 == x2) FillArea(x1, y1 < y2 ? y1 : y2, th, dy + 1, color);
    else if (y1 == y2) FillArea(x1 < x2 ? x1 : x2, y1, dx + 1, th, color);
    else {
        DrawLine(x1, y1, x2, y2, color);
        if (th == 2) {
            if (dy > dx) DrawLine(x1 + 1, y1, x2 + 1, y2, color);
            else DrawLine(x1, y1 + 1, x2, y2 + 1, color);
        }
    }
}

/* widest x with x*x + y*y <= r*r, or -1 when the row is outside the circle */
static int half_width(int y, int r)
{
    int h = 0;
    if (r < 0 || y * y > r * r) return -1;
    while ((h + 1) * (h + 1) + y * y <= r * r) h++;
    return h;
}

static void d_circle(drawing *dr, int cx, int cy, int r, int fill, int outline)
{
    int y;
    (void)dr;
    cx += FE.ox; cy += FE.oy;
    for (y = -r; y <= r; y++) {
        int ho = half_width(y, r), hi = half_width(y, r - 1);
        if (ho < 0) continue;
        if (fill >= 0) FillArea(cx - ho, cy + y, 2 * ho + 1, 1, col(fill));
        if (outline < 0) continue;
        if (hi < 0) FillArea(cx - ho, cy + y, 2 * ho + 1, 1, col(outline));
        else {
            FillArea(cx - ho, cy + y, ho - hi, 1, col(outline));
            FillArea(cx + hi + 1, cy + y, ho - hi, 1, col(outline));
        }
    }
}

static void d_text(drawing *dr, int x, int y, int fonttype, int fontsize, int align, int c, const char *text)
{
    ifont *f = get_font(fonttype == FONT_FIXED, fontsize);
    int sw, left, top, base;
    (void)dr;
    if (!f || !text[0]) return;
    SetFont(f, col(c));
    sw = StringWidth(text);
    base = f->baseline ? f->baseline : f->height * 4 / 5;
    left = FE.ox + x;
    if (align & ALIGN_HCENTRE) left -= sw / 2;
    else if (align & ALIGN_HRIGHT) left -= sw;
    top = FE.oy + y;
    if (align & ALIGN_VCENTRE) top -= base - fontsize * 36 / 100;      /* centre of the digits, not of the line box */
    else top -= base;                                                 /* y is the baseline */
    DrawTextRect(left, top, sw + fontsize, f->height + 2, text, ALIGN_LEFT | VALIGN_TOP);
}

static void d_update(drawing *dr, int x, int y, int w, int h)
{
    (void)dr;
    x += FE.ox; y += FE.oy;
    if (x < FE.ux0) FE.ux0 = x;
    if (y < FE.uy0) FE.uy0 = y;
    if (x + w > FE.ux1) FE.ux1 = x + w;
    if (y + h > FE.uy1) FE.uy1 = y + h;
}

static void d_clip(drawing *dr, int x, int y, int w, int h) { (void)dr; SetClip(FE.ox + x, FE.oy + y, w, h); }
static void d_unclip(drawing *dr) { (void)dr; sx_clip_reset(); }

static void d_start(drawing *dr)
{
    (void)dr;
    FE.ux0 = FE.uy0 = 1 << 30;
    FE.ux1 = FE.uy1 = -1;
}

static void d_end(drawing *dr)
{
    int w = ScreenWidth(), h = ScreenHeight();
    (void)dr;
    sx_clip_reset();
    if (FE.hold || FE.ux1 < 0) return;
    if (FE.ux0 < 0) FE.ux0 = 0;
    if (FE.uy0 < 0) FE.uy0 = 0;
    if (FE.ux1 > w) FE.ux1 = w;
    if (FE.uy1 > h) FE.uy1 = h;
    if (FE.ux1 > FE.ux0 && FE.uy1 > FE.uy0)
        PartialUpdate(FE.ux0, FE.uy0, FE.ux1 - FE.ux0, FE.uy1 - FE.uy0);
}

static void d_status(drawing *dr, const char *text)
{
    (void)dr;
    if (FE.status_cb) FE.status_cb(text);
}

static blitter *d_blit_new(drawing *dr, int w, int h)
{
    blitter *b = snew(blitter);
    (void)dr;
    memset(b, 0, sizeof *b);
    b->w = w; b->h = h;
    return b;
}

static void d_blit_free(drawing *dr, blitter *b)
{
    (void)dr;
    free(b->bm);
    sfree(b);
}

static void d_blit_save(drawing *dr, blitter *b, int x, int y)
{
    int sw = ScreenWidth(), sh = ScreenHeight(), x0, y0, x1, y1;
    (void)dr;
    free(b->bm);
    b->bm = NULL;
    b->x = FE.ox + x; b->y = FE.oy + y;
    x0 = b->x < 0 ? 0 : b->x;
    y0 = b->y < 0 ? 0 : b->y;
    x1 = b->x + b->w > sw ? sw : b->x + b->w;
    y1 = b->y + b->h > sh ? sh : b->y + b->h;
    b->cx = x0; b->cy = y0;
    if (x1 > x0 && y1 > y0) b->bm = BitmapFromScreen(x0, y0, x1 - x0, y1 - y0);
}

static void d_blit_load(drawing *dr, blitter *b, int x, int y)
{
    (void)dr;
    if (!b->bm) return;
    DrawBitmap(FE.ox + x + (b->cx - b->x), FE.oy + y + (b->cy - b->y), b->bm);
}

static char *d_text_fallback(drawing *dr, const char *const *strings, int n)
{
    (void)dr; (void)n;
    return dupstr(strings[0]);
}

static const drawing_api api = {
    .version = 1,
    .draw_text = d_text,
    .draw_rect = d_rect,
    .draw_line = d_line,
    .draw_polygon = draw_polygon_fallback,            /* the engine's own rasteriser, on top of d_rect / d_line */
    .draw_circle = d_circle,
    .draw_update = d_update,
    .clip = d_clip,
    .unclip = d_unclip,
    .start_draw = d_start,
    .end_draw = d_end,
    .status_bar = d_status,
    .blitter_new = d_blit_new,
    .blitter_free = d_blit_free,
    .blitter_save = d_blit_save,
    .blitter_load = d_blit_load,
    .text_fallback = d_text_fallback,
};

/* ---- API for main.c ---- */

midend *fe_open(const game *g)
{
    midend *me;
    FE.timer_on = 0;
    me = midend_new(&FE, g, &api, &FE);
    return me;
}

void fe_close(midend *me)
{
    if (me) midend_free(me);
    FE.status_cb = NULL;
}

void fe_layout(midend *me, int ax, int ay, int aw, int ah, Rect *board)
{
    int w = aw, h = ah;
    midend_size(me, &w, &h, true, 1.0);
    FE.ox = ax + (aw - w) / 2;
    FE.oy = ay + (ah - h) / 2;
    board->x = FE.ox; board->y = FE.oy; board->w = w; board->h = h;
}

void fe_set_thick(int on) { FE.thick = on; }
void fe_hold_updates(int on) { FE.hold = on; }
void fe_set_status_cb(void (*cb)(const char *)) { FE.status_cb = cb; }
int fe_timer_active(void) { return FE.timer_on; }

void fe_drain_timers(midend *me)
{
    int i;
    for (i = 0; i < 4 && FE.timer_on; i++) midend_timer(me, 100.0f);
}

const char *fe_slug(const game *g) { return g->htmlhelp_topic; }
