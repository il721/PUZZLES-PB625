/* Host implementation of the inkview subset used by apps/tatham (see fake/inkview.h).
 * Keeps an 8-bit gray framebuffer with clipping and runs a script (env FAKE_SCRIPT):
 *   size W H            screen size (before "show")
 *   show                EVT_INIT (first time) + EVT_SHOW
 *   tap X Y             press and release at X Y
 *   tapl TEXT           tap the centre of the topmost (last drawn) text equal to TEXT
 *   tapo TEXT           tap the text when it is on the screen, otherwise do nothing
 *   tapfind TEXT        like tapl, but pages forward then back through lists until TEXT is found
 *   drag X1 Y1 X2 Y2   finger down, moves, up
 *   hold X Y            finger down, long press event, up
 *   rtap N SEED X0 Y0 X1 Y1   N taps at random places in the rectangle (every fourth one a hold)
 *   key K               EVT_KEYPRESS
 *   orient N            EVT_ORIENTATION (the app calls SetOrientation)
 *   kbtitle TEXT / kbtype TEXT / kbcancel   the open on-screen keyboard
 *   pgm FILE            write the screen as a PGM image
 *   expect TEXT / expect_not TEXT   TEXT is / is not drawn (substring of a drawn text)
 *   expect_count N TEXT exactly N drawn texts equal TEXT
 *   expect_ink X Y W H MIN MAX   share of non-white pixels (in 1/10000) in the rectangle is within MIN..MAX
 *   expect_full MIN     FullUpdate was called at least MIN times
 *   expect_closed       the app called CloseApp
 *   sh CMD / shnot CMD  run a shell command; fails when it exits non-zero / zero
 *   expect_no_trunc     no text was cut off since the last reset_trunc (or since the start)
 *   reset_trunc         forget earlier cuts
 *   texts               print every text drawn on the screen (debugging)
 *   log TEXT            print a line
 *   quit                EVT_EXIT
 * Text: with -DUSE_FT the Liberation fonts are measured and drawn with FreeType (real glyphs, like the reader's fonts);
 * without it metrics are approximate (0.56 em per character, line height 1.2 em) and text is drawn as bars.
 * DrawTextRect wraps words to the rectangle and, like the reader, cuts what does not fit ("..." with DOTS);
 * every cut is counted (expect_no_trunc). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inkview.h"

#ifdef USE_FT
#include <ft2build.h>
#include FT_FREETYPE_H
static FT_Library ftlib;
#endif

typedef struct { int x, y, w, h; char text[256]; } TextOp;
#define MAXTEXT 6000
static TextOp texts[MAXTEXT];
static int ntexts;
static int W = 1072, H = 1448, closed, failed;
static unsigned char *fb;
static int cx0, cy0, cx1, cy1;
static iv_handler handler;
static const ifont *cur_font;
static int cur_color;
static int full_updates, partial_updates, truncated;
static struct { iv_keyboardhandler h; char *buf; int max; char title[200]; int open; } kb;

void CloseApp(void) { closed = 1; }
int ScreenWidth(void) { return W; }
int ScreenHeight(void) { return H; }
void InkViewMain(iv_handler h) { handler = h; }
void SetPanelType(int t) { (void)t; }

static void alloc_fb(void)
{
    free(fb);
    fb = malloc((size_t)W * H);
    memset(fb, 255, (size_t)W * H);
    cx0 = cy0 = 0; cx1 = W; cy1 = H;
}

void SetOrientation(int n)
{
    static int cur;
    if ((n & 1) != (cur & 1)) { int t = W; W = H; H = t; alloc_fb(); }
    cur = n;
}

static void bad(const char *fmt, const char *a)
{
    fprintf(stderr, "FAIL: ");
    fprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    failed = 1;
}

void FullUpdate(void) { full_updates++; }
void PartialUpdate(int x, int y, int w, int h)
{
    partial_updates++;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > W || y + h > H) {
        char b[100];
        snprintf(b, sizeof b, "PartialUpdate(%d,%d,%d,%d) outside the %dx%d screen", x, y, w, h, W, H);
        bad("%s", b);
    }
}

void SetClip(int x, int y, int w, int h)
{
    cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
    cx1 = x + w > W ? W : x + w; cy1 = y + h > H ? H : y + h;
}

static int gray(int c) { return (c >> 8) & 0xff; }

static void put(int x, int y, int g)
{
    if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1) return;
    fb[(size_t)y * W + x] = (unsigned char)g;
}

void FillArea(int x, int y, int w, int h, int color)
{
    int i, j, g = gray(color);
    if (w <= 0 || h <= 0) return;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) put(i, j, g);
    if ((long)w * h >= 20000) {                         /* a big fill removes the texts under it */
        int k, n = 0;
        for (k = 0; k < ntexts; k++) {
            TextOp *t = &texts[k];
            if (!(t->x >= x && t->y >= y && t->x + t->w <= x + w && t->y + t->h <= y + h)) texts[n++] = *t;
        }
        ntexts = n;
    }
}

void DrawLine(int x1, int y1, int x2, int y2, int color)
{
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1, dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1, err = dx + dy, g = gray(color);
    for (;;) {
        int e2;
        put(x1, y1, g);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

ifont *OpenFont(const char *name, int size, int aa)
{
    ifont *f = calloc(1, sizeof *f);
    (void)aa;
    f->name = strdup(name);
    f->size = size;
    f->height = size * 12 / 10;
    f->baseline = size * 8 / 10;
#ifdef USE_FT
    {
        char path[300];
        FT_Face face;
        if (!ftlib) FT_Init_FreeType(&ftlib);
        snprintf(path, sizeof path, "/usr/share/fonts/truetype/liberation/%s.ttf", strcmp(name, "LiberationSans") ? name : "LiberationSans-Regular");
        if (FT_New_Face(ftlib, path, 0, &face) == 0) {
            FT_Set_Pixel_Sizes(face, 0, size);
            f->fdata = face;
            f->height = (int)(face->size->metrics.height >> 6);
            f->baseline = (int)(face->size->metrics.ascender >> 6);
        } else if (!strncmp(name, "Liberation", 10)) {            /* a font name the reader may not have: like OpenFont returning NULL */
            free(f->name); free(f);
            return NULL;
        }
    }
#endif
    return f;
}
void CloseFont(ifont *f)
{
#ifdef USE_FT
    if (f->fdata) FT_Done_Face((FT_Face)f->fdata);
#endif
    free(f->name); free(f);
}
void SetFont(const ifont *f, int color) { cur_font = f; cur_color = color; }

static int nchars(const char *s)
{
    int n = 0;
    for (; *s; s++) if ((*s & 0xC0) != 0x80) n++;
    return n;
}

static int next_cp(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    int c = *s++, n = 0;
    if (c >= 0xF0) { c &= 7; n = 3; } else if (c >= 0xE0) { c &= 15; n = 2; } else if (c >= 0xC0) { c &= 31; n = 1; }
    while (n-- > 0 && (*s & 0xC0) == 0x80) c = (c << 6) | (*s++ & 63);
    *p = (const char *)s;
    return c;
}

/* width in pixels of the first n bytes of s in the current font */
static int width_n(const char *s, size_t n)
{
    char tmp[1024];
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    memcpy(tmp, s, n);
    tmp[n] = 0;
#ifdef USE_FT
    if (cur_font->fdata) {
        FT_Face face = (FT_Face)cur_font->fdata;
        const char *p = tmp;
        int w = 0;
        while (*p) {
            int cp = next_cp(&p);
            if (FT_Load_Char(face, cp, FT_LOAD_DEFAULT) == 0) w += (int)(face->glyph->advance.x >> 6);
        }
        return w;
    }
#endif
    return nchars(tmp) * cur_font->size * 56 / 100;
}
int StringWidth(const char *s) { return width_n(s, strlen(s)); }

#define MAXLINES 64
typedef struct { const char *s; size_t n; int dots; } Line;

/* word wrap to width w (a word longer than the width stays on a line of its own) */
static int wrap_lines(const char *s, int w, Line *ln)
{
    int nl = 0;
    const char *p = s;
    while (*p && nl < MAXLINES) {
        const char *start = p, *brk = NULL, *q = p;
        ln[nl].dots = 0;
        while (1) {
            const char *e = q;
            while (*e && *e != ' ') e++;
            if (width_n(start, (size_t)(e - start)) > w && brk) { q = brk; break; }
            if (!*e) { q = e; break; }
            brk = e;
            q = e + 1;
        }
        if (q > start && *q == 0 && !brk) q = start + strlen(start);
        ln[nl].s = start;
        ln[nl].n = (size_t)(q - start);
        while (ln[nl].n && start[ln[nl].n - 1] == ' ') ln[nl].n--;
        nl++;
        p = q;
        while (*p == ' ') p++;
    }
    return nl;
}

int TextRectHeight(int width, const char *s, int flags)
{
    Line ln[MAXLINES];
    (void)flags;
    if (!s[0]) return cur_font->height;
    return wrap_lines(s, width, ln) * cur_font->height;
}

static void draw_glyphs(int x, int base, const char *s, size_t n)
{
    char tmp[1024];
    int g = gray(cur_color);
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    memcpy(tmp, s, n);
    tmp[n] = 0;
#ifdef USE_FT
    if (cur_font->fdata) {
        FT_Face face = (FT_Face)cur_font->fdata;
        const char *p = tmp;
        int pen = x;
        while (*p) {
            int cp = next_cp(&p);
            if (FT_Load_Char(face, cp, FT_LOAD_RENDER) != 0) continue;
            {
                FT_GlyphSlot gl = face->glyph;
                int r, c;
                for (r = 0; r < (int)gl->bitmap.rows; r++)
                    for (c = 0; c < (int)gl->bitmap.width; c++) {
                        int a = gl->bitmap.buffer[r * gl->bitmap.pitch + c], px = pen + gl->bitmap_left + c, py = base - gl->bitmap_top + r;
                        if (a && px >= cx0 && px < cx1 && py >= cy0 && py < cy1) {
                            int old = fb[(size_t)py * W + px];
                            fb[(size_t)py * W + px] = (unsigned char)((old * (255 - a) + g * a) / 255);
                        }
                    }
                pen += (int)(gl->advance.x >> 6);
            }
        }
        return;
    }
#endif
    {
        int i, gw = cur_font->size * 56 / 100 - 2, j, k;
        for (i = 0; i < nchars(tmp); i++)
            for (j = 0; j < cur_font->size * 6 / 10; j++)
                for (k = 0; k < gw; k++) put(x + i * cur_font->size * 56 / 100 + 1 + k, base - cur_font->size * 7 / 10 + j, g < 128 ? 40 : 200);
    }
}

char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags)
{
    Line ln[MAXLINES];
    char cut[1100];
    int nl, maxl, i, lh = cur_font->height, tw = 0, y0, cut_any = 0;
    TextOp *t;
    maxl = h / lh;
    if (maxl < 1) maxl = 1;
    nl = s[0] ? wrap_lines(s, w, ln) : 0;
    if (nl > maxl) { nl = maxl; cut_any = 1; ln[nl - 1].dots = 1; }
    y0 = (flags & VALIGN_MIDDLE) ? y + (h - nl * lh) / 2 : (flags & VALIGN_BOTTOM) ? y + h - nl * lh : y;
    for (i = 0; i < nl; i++) {
        const char *src = ln[i].s;
        size_t n = ln[i].n;
        int lw = width_n(src, n), hx;
        if (lw > w || (ln[i].dots && i == nl - 1 && cut_any)) {      /* does not fit: cut and add "..." when asked */
            if (lw > w) cut_any = 1;
            if (flags & DOTS) {
                while (n > 0 && width_n(src, n) + width_n("...", 3) > w) {
                    n--;
                    while (n > 0 && (src[n] & 0xC0) == 0x80) n--;
                }
                memcpy(cut, src, n);
                memcpy(cut + n, "...", 4);
                src = cut; n += 3; lw = width_n(src, n);
            } else {
                while (n > 0 && lw > w) {
                    n--;
                    while (n > 0 && (src[n] & 0xC0) == 0x80) n--;
                    lw = width_n(src, n);
                }
            }
        }
        if (lw > tw) tw = lw;
        hx = (flags & ALIGN_CENTER) ? x + (w - lw) / 2 : (flags & ALIGN_RIGHT) ? x + w - lw : x;
        draw_glyphs(hx, y0 + i * lh + cur_font->baseline, src, n);
    }
    if (cut_any) truncated++;
    if (ntexts < MAXTEXT) {
        int bx = (flags & ALIGN_CENTER) ? x + (w - tw) / 2 : (flags & ALIGN_RIGHT) ? x + w - tw : x;
        t = &texts[ntexts++];
        t->x = bx; t->y = y0; t->w = tw; t->h = nl ? nl * lh : lh;
        snprintf(t->text, sizeof t->text, "%s", s);
    }
    return NULL;
}

ibitmap *BitmapFromScreen(int x, int y, int w, int h)
{
    ibitmap *b;
    int j;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > W || y + h > H) return NULL;
    b = malloc(sizeof *b + (size_t)w * h);
    b->width = w; b->height = h; b->depth = 8; b->scanline = w;
    for (j = 0; j < h; j++) memcpy(b->data + (size_t)j * w, fb + (size_t)(y + j) * W + x, w);
    return b;
}

void DrawBitmap(int x, int y, const ibitmap *b)
{
    int i, j;
    for (j = 0; j < b->height; j++)
        for (i = 0; i < b->width; i++) put(x + i, y + j, b->data[(size_t)j * b->scanline + i]);
}

void Message(int icon, const char *title, const char *text, int timeout)
{
    (void)timeout;
    printf("message(%d): %s: %s\n", icon, title, text);
}

void OpenKeyboard(const char *title, char *buffer, int maxlen, int flags, iv_keyboardhandler h)
{
    (void)flags;
    kb.h = h; kb.buf = buffer; kb.max = maxlen; kb.open = 1;
    snprintf(kb.title, sizeof kb.title, "%s", title);
}

/* ---- script ---- */

static void send(int type, int a, int b) { if (!failed) handler(type, a, b); }

static void tap_at(int x, int y)
{
    send(EVT_POINTERDOWN, x, y);
    send(EVT_POINTERUP, x, y);
}

static int find_text(const char *t)
{
    int i;
    for (i = ntexts - 1; i >= 0; i--) if (strcmp(texts[i].text, t) == 0) return i;
    return -1;
}

static int has_text(const char *t)
{
    int i;
    for (i = 0; i < ntexts; i++) if (strstr(texts[i].text, t)) return 1;
    return 0;
}

static unsigned long rng_state = 1;
static int rnd(int n)
{
    rng_state = rng_state * 6364136223846793005UL + 1442695040888963407UL;
    return (int)((rng_state >> 33) % (unsigned long)n);
}

static void write_pgm(const char *file)
{
    FILE *f = fopen(file, "wb");
    if (!f) { bad("cannot write %s", file); return; }
    fprintf(f, "P5\n%d %d\n255\n", W, H);
    fwrite(fb, 1, (size_t)W * H, f);
    fclose(f);
}

int app_main(void);                 /* the app's own main(), built with -Dmain=app_main */

int main(void)
{
    const char *sf = getenv("FAKE_SCRIPT");
    FILE *f = sf ? fopen(sf, "r") : NULL;
    char line[1024];
    int lineno = 0, inited = 0;
    if (!f) { fprintf(stderr, "FAKE_SCRIPT not set or unreadable\n"); return 2; }
    alloc_fb();
    app_main();
    while (fgets(line, sizeof line, f) && !failed) {
        char *cmd = line, *arg;
        size_t l = strlen(line);
        lineno++;
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l || line[0] == '#') continue;
        arg = strchr(line, ' ');
        if (arg) *arg++ = 0; else arg = line + l;
        if (!strcmp(cmd, "size")) { sscanf(arg, "%d %d", &W, &H); alloc_fb(); }
        else if (!strcmp(cmd, "show")) {
            if (!inited) { inited = 1; send(EVT_INIT, 0, 0); }
            send(EVT_SHOW, 0, 0);
        }
        else if (!strcmp(cmd, "tap")) { int x, y; sscanf(arg, "%d %d", &x, &y); tap_at(x, y); }
        else if (!strcmp(cmd, "tapo")) {
            int i = find_text(arg);
            if (i >= 0) tap_at(texts[i].x + texts[i].w / 2, texts[i].y + texts[i].h / 2);
        }
        else if (!strcmp(cmd, "tapl") || !strcmp(cmd, "tapfind")) {
            int i = find_text(arg), tries = 0;
            while (i < 0 && !strcmp(cmd, "tapfind") && tries++ < 6) {
                static const char *const nx[] = {"Next >", "Далее >", "Weiter >"};
                static const char *const pv[] = {"< Prev", "< Назад", "< Zurück"};
                int k, j = -1;
                for (k = 0; k < 3 && j < 0; k++) j = find_text(tries <= 3 ? nx[k] : pv[k]);
                if (j < 0) continue;
                tap_at(texts[j].x + texts[j].w / 2, texts[j].y + texts[j].h / 2);
                i = find_text(arg);
            }
            if (i < 0) bad("no text '%s' on the screen", arg);
            else tap_at(texts[i].x + texts[i].w / 2, texts[i].y + texts[i].h / 2);
        }
        else if (!strcmp(cmd, "drag")) {
            int x1, y1, x2, y2, s;
            sscanf(arg, "%d %d %d %d", &x1, &y1, &x2, &y2);
            send(EVT_POINTERDOWN, x1, y1);
            for (s = 1; s <= 4; s++) send(EVT_POINTERDRAG, x1 + (x2 - x1) * s / 4, y1 + (y2 - y1) * s / 4);
            send(EVT_POINTERUP, x2, y2);
        }
        else if (!strcmp(cmd, "hold")) {
            int x, y;
            sscanf(arg, "%d %d", &x, &y);
            send(EVT_POINTERDOWN, x, y); send(EVT_POINTERLONG, x, y); send(EVT_POINTERUP, x, y);
        }
        else if (!strcmp(cmd, "rtap")) {
            int n, seed, x0, y0, x1, y1, i;
            sscanf(arg, "%d %d %d %d %d %d", &n, &seed, &x0, &y0, &x1, &y1);
            rng_state = (unsigned long)seed * 2654435761UL + 1;
            for (i = 0; i < n; i++) {
                int x = x0 + rnd(x1 - x0), y = y0 + rnd(y1 - y0);
                if (i % 4 == 3) { send(EVT_POINTERDOWN, x, y); send(EVT_POINTERLONG, x, y); send(EVT_POINTERUP, x, y); }
                else if (i % 7 == 6) { send(EVT_POINTERDOWN, x, y); send(EVT_POINTERDRAG, x0 + rnd(x1 - x0), y0 + rnd(y1 - y0)); send(EVT_POINTERUP, x0 + rnd(x1 - x0), y0 + rnd(y1 - y0)); }
                else tap_at(x, y);
            }
        }
        else if (!strcmp(cmd, "key")) send(EVT_KEYPRESS, atoi(arg), 0);
        else if (!strcmp(cmd, "orient")) send(EVT_ORIENTATION, atoi(arg), 0);
        else if (!strcmp(cmd, "kbtitle")) { if (!kb.open || strcmp(kb.title, arg)) bad("keyboard title is not '%s'", arg); }
        else if (!strcmp(cmd, "kbtype")) {
            if (!kb.open) bad("%s", "no keyboard open");
            else { snprintf(kb.buf, kb.max, "%s", arg); kb.open = 0; kb.h(kb.buf); }
        }
        else if (!strcmp(cmd, "kbcancel")) { if (kb.open) { kb.open = 0; kb.h(NULL); } }
        else if (!strcmp(cmd, "pgm")) write_pgm(arg);
        else if (!strcmp(cmd, "expect")) { if (!has_text(arg)) bad("expected text '%s' not on the screen", arg); }
        else if (!strcmp(cmd, "expect_not")) { if (has_text(arg)) bad("text '%s' should not be on the screen", arg); }
        else if (!strcmp(cmd, "expect_count")) {
            int n, i, c = 0;
            char *t = strchr(arg, ' ');
            if (!t) { bad("%s", "expect_count N TEXT"); continue; }
            *t++ = 0;
            n = atoi(arg);
            for (i = 0; i < ntexts; i++) if (!strcmp(texts[i].text, t)) c++;
            if (c != n) { char b[300]; snprintf(b, sizeof b, "'%s' drawn %d times, expected %d", t, c, n); bad("%s", b); }
        }
        else if (!strcmp(cmd, "expect_ink")) {
            int x, y, w, h, mn, mx, i, j;
            long dark = 0;
            sscanf(arg, "%d %d %d %d %d %d", &x, &y, &w, &h, &mn, &mx);
            for (j = y; j < y + h && j < H; j++)
                for (i = x; i < x + w && i < W; i++) if (fb[(size_t)j * W + i] < 240) dark++;
            {
                int pct = (int)(dark * 10000 / ((long)w * h));
                if (pct < mn || pct > mx) { char b[200]; snprintf(b, sizeof b, "ink %d/10000 outside %d..%d in %s", pct, mn, mx, arg); bad("%s", b); }
            }
        }
        else if (!strcmp(cmd, "expect_full")) { if (full_updates < atoi(arg)) bad("FullUpdate called fewer than %s times", arg); }
        else if (!strcmp(cmd, "expect_closed")) { if (!closed) bad("%s", "the app did not call CloseApp"); }
        else if (!strcmp(cmd, "sh")) { if (system(arg) != 0) bad("command failed: %s", arg); }
        else if (!strcmp(cmd, "shnot")) { if (system(arg) == 0) bad("command should have failed: %s", arg); }
        else if (!strcmp(cmd, "expect_no_trunc")) { if (truncated) { char b[100]; snprintf(b, sizeof b, "%d texts did not fit their rectangle", truncated); bad("%s", b); } }
        else if (!strcmp(cmd, "reset_trunc")) truncated = 0;
        else if (!strcmp(cmd, "texts")) { int i; for (i = 0; i < ntexts; i++) printf("  [%d,%d %dx%d] %s\n", texts[i].x, texts[i].y, texts[i].w, texts[i].h, texts[i].text); }
        else if (!strcmp(cmd, "log")) printf("%s\n", arg);
        else if (!strcmp(cmd, "quit")) send(EVT_EXIT, 0, 0);
        else bad("unknown script command '%s'", cmd);
        if (failed) fprintf(stderr, "  at script line %d: %s %s\n", lineno, cmd, arg);
    }
    fclose(f);
    printf("%s (%d full updates, %d partial updates)\n", failed ? "FAILED" : "ok", full_updates, partial_updates);
    return failed ? 1 : 0;
}
