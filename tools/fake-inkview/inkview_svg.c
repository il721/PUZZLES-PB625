/* Host implementation of the inkview subset: draws into an SVG. Driver script (env REBUS_SCRIPT):
 *   size W H      set the screen size (before the first line that draws)
 *   show          send EVT_INIT (first time) + EVT_SHOW
 *   tap X Y       EVT_POINTERUP
 *   key K         EVT_KEYPRESS (K = number)
 *   orient N      EVT_ORIENTATION (swaps W and H when N is odd)
 *   tapl TEXT     EVT_POINTERUP at the centre of the first text drawn as exactly TEXT
 *   tapd K        EVT_POINTERUP at the centre of the K-th (from 0) text that is a single digit, in drawing order
 *                 (Domino Solitaire draws its 56 cell digits row by row and no other single digits)
 *   tapi K        EVT_POINTERUP at the centre of the K-th (from 0) text drawn, in drawing order (Alphabet Labyrinth: the 64
 *                 cell letters follow the title and the 7 button labels, so cell n is item 8 + n)
 *   svg FILE      write the current screen
 *   expect TEXT   fail unless TEXT is drawn on the current screen
 *   expect_not TEXT  fail if TEXT is drawn on the current screen
 *   expect_closed fail unless the app called CloseApp
 *   quit          EVT_EXIT
 * Text is approximated: width = 0.58 * size per Latin char (0.62 per other UTF-8 char), height = 1.2 * size. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inkview.h"

static int W = 600, H = 800, closed, failed;
static iv_handler handler;
static ifont *cur_font;
static int cur_color;
static char *svg;
static struct { char t[128]; int cx, cy; } items[1024];
static int nitems;
static size_t svg_len, svg_cap;

static void emit(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void emit(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (svg_len + n + 1 > svg_cap) { svg_cap = (svg_len + n + 1) * 2; svg = realloc(svg, svg_cap); }
  memcpy(svg + svg_len, buf, n + 1);
  svg_len += n;
}

static void col(char *out, int c) { sprintf(out, "#%06x", c & 0xffffff); }

void CloseApp(void) { closed = 1; }
int ScreenWidth(void) { return W; }
int ScreenHeight(void) { return H; }
void SetOrientation(int n) { (void)n; }
void ClearScreen(void) { svg_len = 0; nitems = 0; if (svg) svg[0] = 0; }
void FullUpdate(void) {}
void PartialUpdate(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void InkViewMain(iv_handler h) { handler = h; }

void FillArea(int x, int y, int w, int h, int color) {
  char c[8]; col(c, color);
  emit("<rect x='%d' y='%d' width='%d' height='%d' fill='%s'/>\n", x, y, w, h, c);
}
void DrawRect(int x, int y, int w, int h, int color) {
  char c[8]; col(c, color);
  emit("<rect x='%d.5' y='%d.5' width='%d' height='%d' fill='none' stroke='%s'/>\n", x, y, w - 1, h - 1, c);
}
void DrawLine(int x1, int y1, int x2, int y2, int color) {
  char c[8]; col(c, color);
  emit("<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='%s'/>\n", x1, y1, x2, y2, c);
}

ifont *OpenFont(const char *name, int size, int aa) {
  ifont *f = calloc(1, sizeof *f);
  (void)aa;
  f->name = strdup(name); f->size = size; f->height = size * 12 / 10; f->isbold = strstr(name, "Bold") != NULL;
  return f;
}
void CloseFont(ifont *f) { free(f->name); free(f); }
void SetFont(ifont *f, int color) { cur_font = f; cur_color = color; }
/* texts are UTF-8: count characters, not bytes */
/* width in 1/100 px per font size unit: 0.58 em per Latin character, 0.62 em per Cyrillic/other (measured on Liberation Sans) */
static int wunits(const char *s, size_t n) {
  size_t i, w = 0;
  for (i = 0; i < n; i++) {
    if (((unsigned char)s[i] & 0xC0) == 0x80) continue;
    w += ((unsigned char)s[i] < 0x80) ? 58 : 62;
  }
  return (int)w;
}
int StringWidth(const char *s) { return wunits(s, strlen(s)) * cur_font->size / 100; }

static void esc(char *out, const char *s, size_t n) {
  size_t i, o = 0;
  for (i = 0; i < n; i++) {
    if (s[i] == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
    else if (s[i] == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
    else out[o++] = s[i];
  }
  out[o] = 0;
}

static void text_at(int x, int y, const char *s, size_t n) {
  char c[8], e[512];
  col(c, cur_color);
  esc(e, s, n);
  if (nitems < 1024 && n < 128) {
    memcpy(items[nitems].t, s, n); items[nitems].t[n] = 0;
    items[nitems].cx = x + wunits(s, n) * cur_font->size / 100 / 2;
    items[nitems].cy = y + cur_font->height / 2;
    nitems++;
  }
  emit("<text x='%d' y='%d' font-family='Liberation Sans, DejaVu Sans, sans-serif' font-size='%d' font-weight='%s' fill='%s' xml:space='preserve'>%s</text>\n",
       x, y + cur_font->size, cur_font->size, cur_font->isbold ? "bold" : "normal", c, e);
}

void DrawString(int x, int y, const char *s) { text_at(x, y, s, strlen(s)); }

char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags) {
  /* greedy word wrap, '\n' breaks; ALIGN_CENTER / VALIGN_MIDDLE honoured for a single line */
  const char *p = s;
  int ly = y, lh = cur_font->height, lines = 0;
  char line[512];
  size_t ln = 0;
  int total = 0;
  for (int pass = 0; pass < 2; pass++) {
    p = s; ln = 0; lines = 0; ly = y;
    if (pass == 1 && (flags & VALIGN_MIDDLE)) ly = y + (h - total * lh) / 2;
    for (;;) {
      const char *q = p;
      while (*q && *q != ' ' && *q != '\n') q++;
      size_t wl = (size_t)(q - p);
      int need = (wunits(line, ln) + wunits(p, wl) + (ln ? 58 : 0)) * cur_font->size / 100;
      if (ln && need > w) {
        line[ln] = 0;
        if (pass == 1) {
          int lw = wunits(line, ln) * cur_font->size / 100;
          text_at((flags & ALIGN_CENTER) ? x + (w - lw) / 2 : x, ly, line, ln);
        }
        ly += lh; lines++; ln = 0;
      }
      if (ln) line[ln++] = ' ';
      memcpy(line + ln, p, wl); ln += wl;
      if (*q == '\n' || !*q) {
        line[ln] = 0;
        if (pass == 1) {
          int lw = wunits(line, ln) * cur_font->size / 100;
          text_at((flags & ALIGN_CENTER) ? x + (w - lw) / 2 : x, ly, line, ln);
        }
        ly += lh; lines++; ln = 0;
        if (!*q) break;
      }
      p = q + 1;
    }
    total = lines;
  }
  return NULL;
}

static void write_svg(const char *file) {
  FILE *f = fopen(file, "w");
  if (!f) { perror(file); exit(2); }
  fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' viewBox='0 0 %d %d'>\n<rect width='%d' height='%d' fill='white'/>\n%s</svg>\n",
          W, H, W, H, W, H, svg ? svg : "");
  fclose(f);
}

int main(void) {
  const char *sp = getenv("REBUS_SCRIPT");
  FILE *f;
  char line[512];
  int inited = 0;
  extern int rebus_main(void);
  if (!sp || !(f = fopen(sp, "r"))) { fprintf(stderr, "REBUS_SCRIPT missing\n"); return 2; }
  rebus_main(); /* registers the handler through InkViewMain */
  while (fgets(line, sizeof line, f)) {
    int a, b;
    char arg[400];
    line[strcspn(line, "\n")] = 0;
    if (!line[0] || line[0] == '#') continue;
    if (closed && strcmp(line, "expect_closed")) break;
    if (sscanf(line, "size %d %d", &a, &b) == 2) { W = a; H = b; }
    else if (!strcmp(line, "show")) { if (!inited) { handler(EVT_INIT, 0, 0); inited = 1; } handler(EVT_SHOW, 0, 0); }
    else if (sscanf(line, "tap %d %d", &a, &b) == 2) handler(EVT_POINTERUP, a, b);
    else if (!strncmp(line, "tapl ", 5)) {
      int i, hit = 0;
      for (i = 0; i < nitems && !hit; i++)
        if (!strcmp(items[i].t, line + 5)) { handler(EVT_POINTERUP, items[i].cx, items[i].cy); hit = 1; }
      if (!hit) { printf("FAIL tapl: no text '%s'\n", line + 5); failed++; }
    }
    else if (sscanf(line, "tapd %d", &a) == 1) {
      int i, k = 0, hit = 0;
      for (i = 0; i < nitems && !hit; i++)
        if (items[i].t[0] >= '0' && items[i].t[0] <= '9' && !items[i].t[1] && k++ == a) { handler(EVT_POINTERUP, items[i].cx, items[i].cy); hit = 1; }
      if (!hit) { printf("FAIL tapd: no digit item %d\n", a); failed++; }
    }
    else if (sscanf(line, "tapi %d", &a) == 1) {
      if (a >= 0 && a < nitems) handler(EVT_POINTERUP, items[a].cx, items[a].cy);
      else { printf("FAIL tapi: no item %d\n", a); failed++; }
    }
    else if (sscanf(line, "key %d", &a) == 1) handler(EVT_KEYPRESS, a, 0);
    else if (sscanf(line, "orient %d", &a) == 1) { if (a & 1) { int t = W; W = H; H = t; } handler(EVT_ORIENTATION, a, 0); }
    else if (sscanf(line, "svg %399s", arg) == 1) write_svg(arg);
    else if (!strncmp(line, "expect_not ", 11)) {
      if (svg && strstr(svg, line + 11)) { printf("FAIL expect_not '%s'\n", line + 11); failed++; }
    }
    else if (!strncmp(line, "expect ", 7)) {
      if (!svg || !strstr(svg, line + 7)) { printf("FAIL expect '%s'\n", line + 7); failed++; }
    }
    else if (!strcmp(line, "expect_closed")) { if (!closed) { printf("FAIL expect_closed\n"); failed++; } }
    else if (!strcmp(line, "quit")) { handler(EVT_EXIT, 0, 0); break; }
    else { printf("bad script line: %s\n", line); failed++; }
  }
  return failed != 0;
}
