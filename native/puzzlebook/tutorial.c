#include <stdio.h>
#include <string.h>

#include "tutorial.h"

const Str TUT_S_GUIDE = S("How to solve", "Как решать", "So wird gelöst");
const Str TUT_S_HINT = S("List > How to solve: step-by-step guide.", "Список > «Как решать»: разбор по шагам.",
                         "Liste > „So wird gelöst“: Schritt für Schritt.");
static const Str S_NEXT = S("Next", "Далее", "Weiter");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_DONE = S("Done", "Готово", "Fertig");
static const Str S_SKIP = S("Skip", "Пропустить", "Überspringen");
static const Str S_STEP = S("Step %d of %d", "Шаг %d из %d", "Schritt %d von %d");

const char *tut_text(int tm, int step) { return tr(&TUT_TEXTS[tm][step]); }

/* lines the text needs in a rectangle w wide when set in font f (greedy wrap at spaces, like DrawTextRect) */
static int lines_needed(ifont *f, const char *s, int w) {
  char word[160];
  int lines = 1, x = 0, sp, n;
  SetFont(f, BLACK);
  sp = StringWidth(" ");
  while (*s) {
    n = 0;
    while (*s && *s != ' ' && n < (int)sizeof word - 1) word[n++] = *s++;
    word[n] = 0;
    while (*s == ' ') s++;
    if (n) {
      int ww = StringWidth(word);
      if (x > 0 && x + sp + ww > w) { lines++; x = ww; }
      else x += (x > 0 ? sp : 0) + ww;
    }
  }
  return lines;
}

void tut_layout(int tm, int step, int W, int H, TutPanel *p) {
  int m = 8, gap = 6, bh = 64, i, maxl = 0, texth, bw;
  ifont *f = ui_f_small;
  p->tx = 12; p->tw = W - 24;
  /* the font and the height are those the longest step of the module needs, so the board stays where it is */
  for (i = 0; i < TUT_COUNT[tm]; i++) {
    int l = lines_needed(f, tr(&TUT_TEXTS[tm][i]), p->tw);
    if (l > maxl) maxl = l;
  }
  if (maxl * f->height > 250) {
    f = ui_f_tiny;
    maxl = 0;
    for (i = 0; i < TUT_COUNT[tm]; i++) {
      int l = lines_needed(f, tr(&TUT_TEXTS[tm][i]), p->tw);
      if (l > maxl) maxl = l;
    }
  }
  p->font = f;
  texth = maxl * f->height + f->height / 2;
  bw = (W - 2 * m - gap) / 2;
  p->th = texth;
  p->ty = H - m - bh - gap - texth;
  p->top = p->ty - gap;
  p->nbtn = 0;
  p->btn[p->nbtn].x = W - m - 150; p->btn[p->nbtn].y = m; p->btn[p->nbtn].w = 150; p->btn[p->nbtn].h = 46;
  p->btn[p->nbtn].id = TUT_B_SKIP; p->btn[p->nbtn].label = tr(&S_SKIP);
  p->nbtn++;
  if (step > 0) {
    p->btn[p->nbtn].x = m; p->btn[p->nbtn].y = H - m - bh; p->btn[p->nbtn].w = bw; p->btn[p->nbtn].h = bh;
    p->btn[p->nbtn].id = TUT_B_BACK; p->btn[p->nbtn].label = tr(&S_BACK);
    p->nbtn++;
  }
  p->btn[p->nbtn].x = m + bw + gap; p->btn[p->nbtn].y = H - m - bh; p->btn[p->nbtn].w = bw; p->btn[p->nbtn].h = bh;
  p->btn[p->nbtn].id = TUT_B_NEXT; p->btn[p->nbtn].label = tr(step == TUT_COUNT[tm] - 1 ? &S_DONE : &S_NEXT);
  p->nbtn++;
}

void tut_draw(int tm, int step, int W, const TutPanel *p) {
  char t[48];
  int i;
  snprintf(t, sizeof t, tr(&S_STEP), step + 1, TUT_COUNT[tm]);
  ui_text_center(ui_f_title, BLACK, 8, 8, W - 16 - 150 - 8, 46, t);
  for (i = 0; i < p->nbtn; i++) ui_draw_btn(&p->btn[i]);
  DrawLine(8, p->top, W - 8, p->top, BLACK);
  SetFont(p->font, BLACK);
  DrawTextRect(p->tx, p->ty, p->tw, p->th, (char *)tut_text(tm, step), ALIGN_LEFT | VALIGN_MIDDLE);
}

int tut_next_step(int tm, int step, int id) {
  if (id == TUT_B_BACK) return step > 0 ? step - 1 : 0;
  if (id == TUT_B_NEXT) return step + 1 < TUT_COUNT[tm] ? step + 1 : -1;
  return -1;
}
