/* Keyword Rebuses: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: keyword_core.c / keyword_data.h (port of il721/puzzles, module_keyword_rebus).
 * UI: the 3x3 cross of letter-coded words (signs between them), under it the letter bank (the ten letters, each with
 * its digit once it has one) and the keyword strip (slots 0..9: the letter that holds each digit), then the digit pad.
 * Tap a letter (grid, bank, or a strip slot), then a digit; Del removes the digit of the selected letter. A digit on
 * two letters turns both black, a word of 2+ letters starting with 0 turns its first letter black, a failing equation
 * (all letters set) turns its signs black. Portrait layout only (the reader does not rotate). Progress in /mnt/ext1/keyword.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "../tutorial.h"
#include "keyword_core.h"
#include "keyword_data.h"
#include "keyword_tutorial.h"

#ifndef KEYWORDSAVEF
#define KEYWORDSAVEF "/mnt/ext1/keyword.sav"
#endif

enum { B_RESET = 10, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE, B_DEL, B_DIG = 20, B_LET = 30, B_SLOT = 40 };
enum { M_PLAY, M_LIST, M_HELP, M_GUIDE };

typedef UiBtn Btn;
typedef struct { ifont *big, *small; int size; } FontPair;

static KwBoard boards[KNUM_PUZZLES];
static int solved[KNUM_PUZZLES];
static long long stamp[KNUM_PUZZLES]; /* ms since 1970 of the last change to each puzzle, 0 = unknown */
static int cur = 1, mode = M_PLAY;
static int reset_armed, updates;
static char msg[200];
static KwBoard gboard;              /* the walkthrough board, rebuilt for every redraw; never saved */
static uint8_t gemph[3][3];         /* words the step emphasises */
static int gstep;                  /* guided walkthrough (mode M_GUIDE): the step, 0-based */
static TutPanel gpanel;
static int left; /* set when the module handed control back to the shell */

static Btn btns[48];
static int nbtn;
static int title_x, title_w;                 /* title strip in the top bar */
static int gx, gy, gt, sg, lane;             /* grid origin, word tile size, sign lane width, sign lane height */
static int colw[3], colx[3], rowy[3], ly0, ly1; /* letters per column, column x, word row y, y of the operator / equals lanes */
static int bank_x, bank_y, strip_y, ts;      /* letter bank and keyword strip: first tile x, row y, tile size */
static int st_x, st_y, st_w, st_h;           /* status area */
static char letter_txt[KW_LETTERS][4];       /* UTF-8 of the current puzzle's letters */
static char digit_txt[10][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

static FontPair fg, fb; /* grid tiles / bank and strip tiles */

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_DEL = S("Del", "Стереть", "Löschen");
static const Str S_EXAMPLE = S("Example  (1/%d)", "Пример  (1/%d)", "Beispiel  (1/%d)");
static const Str S_PUZZLE = S("Keyword Rebus %d/%d%s", "Ребус с ключом %d/%d%s", "Rechenrätsel %d/%d%s");
static const Str S_EX_SHORT = S("Ex", "Пр", "Bsp");
static const Str S_LIST_TITLE = S("Keyword Rebuses - choose a puzzle", "Выберите ребус", "Rätsel wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_SET = S("Letters with a digit: %d of 10", "Букв с цифрой: %d из 10", "Buchstaben mit Ziffer: %d von 10");
static const Str S_CONFLICT = S("A digit is on two letters - both are shown in black.",
                                "Одна цифра у двух букв — обе буквы выделены чёрным.",
                                "Eine Ziffer gehört zu zwei Buchstaben — beide sind schwarz markiert.");
static const Str S_LEAD0 = S("A word of several letters cannot start with 0 - its first letter is shown in black.",
                             "Слово из нескольких букв не может начинаться с нуля — его первая буква выделена чёрным.",
                             "Ein Wort aus mehreren Buchstaben beginnt nicht mit 0 — sein erster Buchstabe ist schwarz markiert.");
static const Str S_WRONG = S("Black signs: that equation does not hold.", "Чёрные знаки: этот пример не сходится.",
                             "Schwarze Zeichen: diese Aufgabe geht nicht auf.");
static const Str S_PICK_LETTER = S("Tap a letter first.", "Сначала нажмите на букву.", "Tippen Sie zuerst einen Buchstaben an.");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решено. Меняйте или нажмите «Сбросить».",
                                     "Schon gelöst. Frei ändern oder „Reset“ tippen.");
static const Str S_SOLVED = S("All six equations check out - the keyword is \"%s\". Press > for the next puzzle.",
                              "Все шесть примеров сошлись — ключевое слово: «%s». Нажмите >, чтобы перейти к следующему ребусу.",
                              "Alle sechs Aufgaben gehen auf — das Schlüsselwort lautet „%s“. Mit > geht es zum nächsten Rätsel.");
static const Str S_RESET_AGAIN = S("Tap Reset again to remove all digits.", "Нажмите «Сбросить» ещё раз, чтобы убрать все цифры.",
                                   "Noch einmal „Reset“ tippen, um alle Ziffern zu löschen.");

static const Str S_HELP_TEXT = S(
    "A 3x3 square of words built from ten different letters. Each letter is one digit: the same letter is the same digit, "
    "different letters are different digits. A word of several letters never starts with 0.\n\n"
    "Every row and every column is an equation: the first two numbers give the third. Row 1 multiplies, rows 2 and 3 add; "
    "the columns add, multiply and subtract (top minus middle).\n\n"
    "Tap a letter (grid, letter bank or strip), then a digit. Del removes the digit. A digit on two letters turns both "
    "black; a failing equation turns its signs black. When all ten letters hold different digits and all six equations "
    "hold, the letters ordered by their digits 0-9 spell the keyword.\n\n"
    "Tap anywhere to close.",
    "Квадрат 3×3 из слов, составленных из десяти разных букв. Каждая буква — одна цифра: одинаковые буквы — одинаковые "
    "цифры, разные — разные. Слово из нескольких букв не начинается с нуля.\n\n"
    "Каждая строка и каждый столбец — пример: первые два числа дают третье. В первой строке числа умножаются, во второй и "
    "третьей складываются; в столбцах: сложение, умножение, вычитание (верхнее минус среднее).\n\n"
    "Нажмите на букву (в сетке, банке букв или полоске), затем на цифру. «Стереть» убирает цифру. Цифра у двух букв делает "
    "обе чёрными; не сошедшийся пример чернит свои знаки. Когда у десяти букв разные цифры и все шесть примеров сходятся, "
    "буквы в порядке цифр 0–9 дают ключевое слово.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Ein 3x3-Quadrat aus Wörtern aus zehn verschiedenen Buchstaben. Jeder Buchstabe ist eine Ziffer: gleiche Buchstaben "
    "gleiche Ziffern, verschiedene Buchstaben verschiedene. Ein Wort aus mehreren Buchstaben beginnt nie mit 0.\n\n"
    "Jede Zeile und Spalte ist eine Aufgabe: Die ersten beiden Zahlen ergeben die dritte. Zeile 1 multipliziert, Zeile 2 "
    "und 3 addieren; die Spalten addieren, multiplizieren und subtrahieren (oben minus Mitte).\n\n"
    "Tippen Sie einen Buchstaben an (Gitter, Bank oder Streifen), dann eine Ziffer. „Löschen“ entfernt die Ziffer. Eine "
    "Ziffer bei zwei Buchstaben färbt beide schwarz; eine falsche Aufgabe färbt ihre Zeichen schwarz. Haben alle zehn "
    "Buchstaben verschiedene Ziffern und gehen alle sechs Aufgaben auf, ergeben die Buchstaben nach Ziffern 0-9 das "
    "Schlüsselwort.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(KEYWORDSAVEF, "w");
  int i;
  char buf[KW_SAVE_LEN + 1];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < KNUM_PUZZLES; i++) {
    kw_save(&boards[i], buf);
    fprintf(f, "%d %s %lld\n", solved[i], buf, stamp[i]);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(KEYWORDSAVEF, "r");
  char line[KW_SAVE_LEN + 64];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < KNUM_PUZZLES) cur = v;
  }
  for (i = 0; i < KNUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *sp = strchr(line, ' ');
    char *sp2;
    if (!sp) continue;
    sp2 = strchr(sp + 1, ' ');
    if (sp2) *sp2 = 0;
    solved[i] = atoi(line) ? 1 : 0;
    kw_load(&boards[i], sp + 1);
    stamp[i] = sp2 ? strtoll(sp2 + 1, NULL, 10) : 0;
  }
  fclose(f);
}

/* ---------- fonts ---------- */

static void ensure_fonts(FontPair *fp, int size) {
  int big = size * 52 / 100, small = size * 28 / 100;
  if (fp->big && fp->size == size) return;
  if (fp->big) CloseFont(fp->big);
  if (fp->small) CloseFont(fp->small);
  fp->big = ui_open_font("LiberationSans-Bold", big > 16 ? big : 16);
  fp->small = ui_open_font("LiberationSans-Bold", small > 12 ? small : 12);
  fp->size = size;
}

/* ---------- layout (portrait) ---------- */

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

/* the biggest word tile (at most tmax) whose cross fits availW x availH; sets gt, sg, lane and the column widths */
static const KwBoard *view(void) { return mode == M_GUIDE ? &gboard : &boards[cur]; }

static void grid_size(int availW, int availH, int tmax) {
  const KwPuzzle *p = view()->p;
  int r, c, t;
  for (c = 0; c < 3; c++) {
    colw[c] = 1;
    for (r = 0; r < 3; r++) if (p->len[r][c] > colw[c]) colw[c] = p->len[r][c];
  }
  for (t = tmax; t > 20; t--) {
    int s = t * 7 / 10, l = t * 6 / 10;
    if ((colw[0] + colw[1] + colw[2]) * t + 2 * s <= availW && 3 * t + 2 * l <= availH) break;
  }
  gt = t;
  sg = t * 7 / 10;
  lane = t * 6 / 10;
}

static void grid_place(int x, int y, int availW, int availH) {
  int gw = (colw[0] + colw[1] + colw[2]) * gt + 2 * sg, gh = 3 * gt + 2 * lane;
  gx = x + (availW - gw) / 2;
  gy = y + (availH - gh) / 2;
  colx[0] = gx;
  colx[1] = colx[0] + colw[0] * gt + sg;
  colx[2] = colx[1] + colw[1] * gt + sg;
  rowy[0] = gy;
  ly0 = gy + gt;
  rowy[1] = ly0 + lane;
  ly1 = rowy[1] + gt;
  rowy[2] = ly1 + lane;
}

/* walkthrough: the cross on top, the letter bank and the keyword strip under it (drawn, not buttons), then the step text and
 * its buttons (tutorial.h) */
static void guide_layout(int W, int H) {
  int i, m = 8, availH;
  nbtn = 0;
  tut_layout(TUT_KEYWORD, gstep, W, H, &gpanel);
  for (i = 0; i < gpanel.nbtn; i++) add_btn(gpanel.btn[i].x, gpanel.btn[i].y, gpanel.btn[i].w, gpanel.btn[i].h, gpanel.btn[i].id, gpanel.btn[i].label);
  ts = (W - 2 * m - 9 * 2) / 10;
  if (ts > 60) ts = 60;
  bank_x = (W - (10 * ts + 9 * 2)) / 2;
  strip_y = gpanel.top - 8 - ts;
  bank_y = strip_y - 8 - ts;
  availH = bank_y - 8 - 62;
  grid_size(W - 2 * m, availH, 90);
  grid_place(m, 62, W - 2 * m, availH);
}

static void layout(int W, int H) {
  int m = 8, gap = 6, k;
  int tb = 58;
  int by, bh, bx, bw, ph, pw;
  if (mode == M_GUIDE) { guide_layout(W, H); return; }
  nbtn = 0;

  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;

  {
    /* bottom up: the < Reset > row, the two digit rows (0..5, 6..9 + Del), the strip, the bank, the status; the cross in what is left */
    int top = tb + 6, stat_h = 76, availH;
    bh = 64;
    ph = 60;
    by = H - m - bh;
    bx = m; bw = W - 2 * m;
    ts = (W - 2 * m - 9 * 2) / 10;
    if (ts > 60) ts = 60;
    bank_x = (W - (10 * ts + 9 * 2)) / 2;
    pw = (W - 2 * m - 5 * gap) / 6;
    {
      int py2 = by - 10 - ph, py1 = py2 - gap - ph;
      for (k = 0; k < 6; k++) add_btn(m + k * (pw + gap), py1, pw, ph, B_DIG + k, digit_txt[k]);
      for (k = 6; k < 10; k++) add_btn(m + (k - 6) * (pw + gap), py2, pw, ph, B_DIG + k, digit_txt[k]);
      add_btn(m + 4 * (pw + gap), py2, W - m - (m + 4 * (pw + gap)), ph, B_DEL, tr(&S_DEL));
      strip_y = py1 - 10 - ts;
      bank_y = strip_y - 8 - ts;
    }
    st_x = m; st_w = W - 2 * m; st_h = stat_h;
    st_y = bank_y - 8 - stat_h;
    availH = st_y - 4 - top;
    grid_size(W - 2 * m, availH, 90);
    grid_place(m, top, W - 2 * m, availH);
  }
  for (k = 0; k < KW_LETTERS; k++) add_btn(bank_x + k * (ts + 2), bank_y, ts, ts, B_LET + k, letter_txt[k]);
  for (k = 0; k < 10; k++) add_btn(bank_x + k * (ts + 2), strip_y, ts, ts, B_SLOT + k, digit_txt[k]);
  {
    int w3 = (bw - 2 * gap) / 3;
    add_btn(bx, by, w3, bh, B_PREV, "<");
    add_btn(bx + w3 + gap, by, w3, bh, B_RESET, tr(&S_RESET));
    add_btn(bx + 2 * (w3 + gap), by, w3, bh, B_NEXT, ">");
  }
}

/* ---------- board drawing ---------- */

/* one letter tile of size s at (x, y): the letter, or the digit big with the letter small in the corner once it has one;
 * inverted = black (digit conflict / leading zero); the selected letter has a thick frame and a grey ground */
static void draw_tile(int x, int y, int s, int letter, const FontPair *fp, int inverted) {
  const KwBoard *b = view();
  int d = b->val[letter], sel = (b->sel == letter), ix = x + 1, iy = y + 1, is = s - 2, i;
  int ink = inverted ? WHITE : BLACK;
  FillArea(ix, iy, is, is, inverted ? BLACK : sel ? 0xBBBBBB : WHITE);
  DrawRect(ix, iy, is, is, BLACK);
  if (sel) {
    int th = is >= 48 ? 4 : 3;
    for (i = 0; i < th; i++) DrawRect(ix + i, iy + i, is - 2 * i, is - 2 * i, inverted ? WHITE : BLACK);
  }
  if (d < 0) {
    ui_text_center(fp->big, ink, ix, iy, is, is, letter_txt[letter]);
  } else {
    ui_text_center(fp->big, ink, ix + is / 5, iy + is / 12, is - is / 5, is - is / 12, digit_txt[d]);
    SetFont(fp->small, ink);
    DrawString(ix + 4, iy + 2, letter_txt[letter]);
  }
}

static void draw_sign(int x, int y, int w, int h, const char *sym, int wrong) {
  int side = (w < h ? w : h) - 4, bx = x + (w - side) / 2, by = y + (h - side) / 2;
  if (wrong) FillArea(bx, by, side, side, BLACK);
  ui_text_center(fg.big, wrong ? WHITE : BLACK, x, y, w, h, sym);
}

static const char *op_sym(int op) { return op == '*' ? "×" : op == '+' ? "+" : "−"; }

static void draw_grid(const KwStatus *st) {
  const KwPuzzle *p = view()->p;
  int r, c, k;
  for (r = 0; r < 3; r++) {
    for (c = 0; c < 3; c++) {
      int len = p->len[r][c], x0 = colx[c] + (colw[c] - len) * gt;
      for (k = 0; k < len; k++) {
        int l = p->w[r][c][k];
        draw_tile(x0 + k * gt, rowy[r], gt, l, &fg, st->conflict[l] || (k == 0 && st->lead0[r][c]));
      }
      if (mode == M_GUIDE && gemph[r][c]) { /* the emphasised word gets a thick frame around its tiles */
        int t;
        for (t = 0; t < 3; t++) DrawRect(x0 - 2 - t, rowy[r] - 2 - t, len * gt + 4 + 2 * t, gt + 4 + 2 * t, BLACK);
      }
    }
    draw_sign(colx[0] + colw[0] * gt, rowy[r], sg, gt, op_sym(kw_row_op(r)), st->eq[r] == KW_WRONG);
    draw_sign(colx[1] + colw[1] * gt, rowy[r], sg, gt, "=", st->eq[r] == KW_WRONG);
  }
  for (c = 0; c < 3; c++) {
    draw_sign(colx[c], ly0, colw[c] * gt, lane, op_sym(kw_col_op(c)), st->eq[3 + c] == KW_WRONG);
    draw_sign(colx[c], ly1, colw[c] * gt, lane, "=", st->eq[3 + c] == KW_WRONG);
  }
}

/* the digit pad: black = the selected letter's digit, grey = a digit some letter already holds */
static void draw_pad_btn(const Btn *b, const KwStatus *st) {
  const KwBoard *bd = &boards[cur];
  int d = b->id - B_DIG, inverted = 0, grey = 0;
  if (b->id == B_DEL) { ui_draw_btn(b); return; }
  if (bd->sel >= 0 && bd->val[bd->sel] == d) inverted = 1;
  else if (st->holders[d]) grey = 1;
  ui_box(b->x, b->y, b->w, b->h, 2, inverted ? BLACK : grey ? 0xDDDDDD : WHITE);
  ui_text_center(ui_f_btn, inverted ? WHITE : BLACK, b->x, b->y, b->w, b->h, b->label);
}

/* strip slot d: its digit small in the corner, the letter holding it big, "!" (black) when two letters hold it */
static void draw_slot(const Btn *b, const KwStatus *st) {
  int d = b->id - B_SLOT, x = b->x + 1, y = b->y + 1, s = b->w - 2, dup = st->holders[d] > 1;
  FillArea(x, y, s, s, dup ? BLACK : 0xEEEEEE);
  DrawRect(x, y, s, s, BLACK);
  SetFont(fb.small, dup ? WHITE : BLACK);
  DrawString(x + 4, y + 2, digit_txt[d]);
  if (dup) ui_text_center(fb.big, WHITE, x + s / 5, y + s / 12, s - s / 5, s - s / 12, "!");
  else if (st->holder[d] >= 0) ui_text_center(fb.big, BLACK, x + s / 5, y + s / 12, s - s / 5, s - s / 12, letter_txt[st->holder[d]]);
}

/* ---------- screens ---------- */

static void draw_status(const KwStatus *st) {
  char t[400];
  const char *extra = "";
  int len;
  if (reset_armed) extra = tr(&S_RESET_AGAIN);
  else if (st->solved) {
    uint16_t kw[10];
    char word[24];
    int i, n = 0;
    kw_keyword(&boards[cur], kw);
    for (i = 0; i < 10; i++) { ui_utf8(kw[i], word + n); n += (int)strlen(word + n); }
    snprintf(t, sizeof t, tr(&S_SOLVED), word);
    SetFont(ui_f_small, BLACK);
    DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
    return;
  } else if (msg[0]) extra = msg;
  else if (st->nconflict) extra = tr(&S_CONFLICT);
  else if (st->nlead0) extra = tr(&S_LEAD0);
  else if (st->nwrong) extra = tr(&S_WRONG);
  len = snprintf(t, sizeof t, tr(&S_SET), st->set);
  if (extra[0] && len < (int)sizeof t) snprintf(t + len, sizeof t - (size_t)len, "\n%s", extra);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
}

static void draw_play(void) {
  char t[64];
  const KwPuzzle *p = boards[cur].p;
  KwStatus st;
  ifont *tf = ui_f_title;
  int i;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), KNUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, KNUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  SetFont(tf, BLACK);
  if (StringWidth(t) > title_w) tf = ui_f_small;
  ui_text_center(tf, BLACK, title_x, 8, title_w, btns[0].h, t);
  kw_status(&boards[cur], &st);
  /* drawing order = the order a test script's "tapl" searches: top bar, pad, bank, strip, buttons, then the cross */
  for (i = 0; i < nbtn; i++) {
    const Btn *b = &btns[i];
    if (b->id >= B_DIG && b->id < B_LET) draw_pad_btn(b, &st);
    else if (b->id == B_DEL) draw_pad_btn(b, &st);
    else if (b->id >= B_LET && b->id < B_SLOT) draw_tile(b->x, b->y, b->w, b->id - B_LET, &fb, st.conflict[b->id - B_LET]);
    else if (b->id >= B_SLOT) draw_slot(b, &st);
    else ui_draw_btn(b);
  }
  draw_grid(&st);
  draw_status(&st);
}

static void draw_guide(int W) {
  KwStatus st;
  int k;
  tut_draw(TUT_KEYWORD, gstep, W, &gpanel);
  kw_status(&gboard, &st);
  draw_grid(&st);
  for (k = 0; k < KW_LETTERS; k++) {
    Btn slot;
    draw_tile(bank_x + k * (ts + 2), bank_y, ts, k, &fb, st.conflict[k]);
    slot.x = bank_x + k * (ts + 2); slot.y = strip_y; slot.w = ts; slot.h = ts; slot.id = B_SLOT + k; slot.label = "";
    draw_slot(&slot, &st);
  }
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 10, cols = 4, rows = (KNUM_PUZZLES + cols - 1) / cols;
  int bw = (W - 2 * m - (cols - 1) * gap) / cols, bh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (bh > bw) bh = bw;
  ui_text_center(StringWidth(tr(&S_LIST_TITLE)) > W - 2 * m ? ui_f_small : ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < KNUM_PUZZLES; i++) {
    int x = m + (i % cols) * (bw + gap), y = top + (i / cols) * (bh + gap);
    char t[24];
    if (boards[i].p->tutorial) snprintf(t, sizeof t, "%s%s", tr(&S_EX_SHORT), solved[i] ? " ok" : "");
    else snprintf(t, sizeof t, "%d%s", i, solved[i] ? " ok" : "");
    ui_box(x, y, bw, bh, i == cur ? 4 : 2, solved[i] ? 0xDDDDDD : WHITE);
    ui_text_center(ui_f_btn, BLACK, x, y, bw, bh, t);
    add_btn(x, y, bw, bh, 100 + i, "");
  }
  {
    int bwid = (W - 2 * m - 2 * gap) / 3, by = H - 80; /* Back (to the puzzle), How to solve and Home (to the main menu) */
    add_btn(m, by, bwid, 64, B_CLOSE, tr(&S_BACK));
    add_btn(m + bwid + gap, by, bwid, 64, TUT_B_GUIDE, tr(&TUT_S_GUIDE));
    add_btn(m + 2 * (bwid + gap), by, bwid, 64, B_HOME, tr(&S_HOME));
    for (i = nbtn - 3; i < nbtn; i++) ui_draw_btn(&btns[i]);
  }
}

static void draw_help(int W, int H) {
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_HELP_TITLE));
  SetFont(ui_f_small, BLACK);
  DrawTextRect(16, 64, W - 32, H - 80, (char *)tr(&S_HELP_TEXT), ALIGN_LEFT);
}

static void set_letters(void) {
  const KwPuzzle *p = view()->p;
  int k;
  for (k = 0; k < KW_LETTERS; k++) ui_utf8(p->let[k], letter_txt[k]);
}

static void redraw(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts(28);
  if (mode == M_GUIDE) kt_tut_board(&KPUZZLES[0], gstep, &gboard, gemph);
  if (mode == M_PLAY || mode == M_GUIDE) {
    set_letters();
    layout(W, H);
    ensure_fonts(&fg, gt);
    ensure_fonts(&fb, ts);
  }
  ClearScreen();
  if (mode == M_PLAY) draw_play();
  else if (mode == M_GUIDE) draw_guide(W);
  else if (mode == M_LIST) draw_list(W, H);
  else draw_help(W, H);
  updates++;
  if (full || updates % 8 == 0) FullUpdate();
  else PartialUpdate(0, 0, W, H);
}

/* ---------- actions ---------- */

static void open_puzzle(int i) {
  cur = (i % KNUM_PUZZLES + KNUM_PUZZLES) % KNUM_PUZZLES;
  reset_armed = 0; msg[0] = 0;
  boards[cur].sel = -1;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  else if (boards[cur].p->tutorial) strcpy(msg, tr(&TUT_S_HINT));
  save_state();
}

/* a tap on a letter (bank, strip or grid): select it, or drop the selection when it is already selected */
static void select_letter(int l) {
  KwBoard *b = &boards[cur];
  reset_armed = 0; msg[0] = 0;
  b->sel = (b->sel == l) ? -1 : (int8_t)l;
}

/* a digit button (0..9) or Del (-1); returns 1 if the board is now solved (fresh solve) */
static int put_digit(int digit) {
  KwBoard *b = &boards[cur];
  int won = 0;
  reset_armed = 0; msg[0] = 0;
  if (b->sel < 0) { strcpy(msg, tr(&S_PICK_LETTER)); return 0; }
  if (b->val[(int)b->sel] == digit) return 0;
  if (kw_set(b, b->sel, digit)) {
    stamp[cur] = ui_now_ms();
    if (digit >= 0 && kw_wins(b)) { solved[cur] = 1; won = 1; b->sel = -1; }
    save_state();
  }
  return won;
}

static void do_button(int id) {
  if (id >= 100 && id < 100 + KNUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; }
      else { kw_reset(&boards[cur]); solved[cur] = 0; stamp[cur] = ui_now_ms(); reset_armed = 0; msg[0] = 0; save_state(); }
      break;
    case B_PREV: open_puzzle(cur - 1); break;
    case B_NEXT: open_puzzle(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
    case TUT_B_GUIDE: mode = M_GUIDE; gstep = 0; break;
    case TUT_B_BACK: case TUT_B_NEXT: case TUT_B_SKIP:
      gstep = tut_next_step(TUT_KEYWORD, gstep, id);
      if (gstep < 0) { gstep = 0; mode = M_LIST; }
      break;
    case B_HOME: left = 1; shell_leave(); return;
    default: break;
  }
}

static void tap(int x, int y) {
  int i, r, c, k;
  if (mode == M_HELP) { mode = M_PLAY; redraw(1); return; }
  for (i = 0; i < nbtn; i++) {
    const Btn *b = &btns[i];
    if (ui_hit(b, x, y)) {
      int before = mode, won = 0;
      if (mode == M_PLAY && b->id >= B_DIG && b->id < B_LET) won = put_digit(b->id - B_DIG);
      else if (mode == M_PLAY && b->id == B_DEL) won = put_digit(-1);
      else if (mode == M_PLAY && b->id >= B_LET && b->id < B_SLOT) select_letter(b->id - B_LET);
      else if (mode == M_PLAY && b->id >= B_SLOT && b->id < 100) {
        KwStatus st;
        kw_status(&boards[cur], &st);
        if (st.holder[b->id - B_SLOT] >= 0) select_letter(st.holder[b->id - B_SLOT]);
      } else do_button(b->id);
      if (left) { left = 0; return; }
      redraw(won || mode != before || b->id == B_PREV || b->id == B_NEXT || b->id == B_RESET || b->id >= 100);
      return;
    }
  }
  if (mode != M_PLAY) return;
  for (r = 0; r < 3; r++)
    for (c = 0; c < 3; c++) {
      int len = boards[cur].p->len[r][c], x0 = colx[c] + (colw[c] - len) * gt;
      for (k = 0; k < len; k++)
        if (x >= x0 + k * gt && x < x0 + (k + 1) * gt && y >= rowy[r] && y < rowy[r] + gt) {
          select_letter(boards[cur].p->w[r][c][k]);
          redraw(0);
          return;
        }
    }
}

/* ---------- Module interface (shell.h) ---------- */

static void keyword_init(void) {
  int i;
  for (i = 0; i < KNUM_PUZZLES; i++) kw_init(&boards[i], &KPUZZLES[i]);
  cur = 1;
  load_state();
}

static void keyword_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { reset_armed = 0; msg[0] = 0; boards[cur].sel = -1; }
  redraw(1);
}

static int keyword_event(int type, int par1, int par2) {
  switch (type) {
    case EVT_SHOW:
    case EVT_ORIENTATION:
      redraw(1);
      break;
    case EVT_POINTERUP:
      tap(par1, par2);
      break;
    case EVT_KEYPRESS:
      if (par1 == KEY_BACK) {
        if (mode == M_GUIDE) { mode = M_LIST; redraw(1); }
        else if (mode != M_PLAY) { mode = M_PLAY; redraw(1); }
        else shell_leave();
      } else if (par1 == KEY_HOME || par1 == KEY_MENU) {
        shell_leave(); /* the reader's hardware button: straight back to the home screen */
      } else if (mode == M_GUIDE && (par1 == KEY_PREV || par1 == KEY_NEXT)) {
        /* the reader's page keys step through the walkthrough (the last step is left with its Done button) */
        if (par1 == KEY_PREV) gstep = gstep > 0 ? gstep - 1 : 0;
        else if (gstep + 1 < TUT_COUNT[TUT_KEYWORD]) gstep++;
        redraw(1);
      } else if (par1 == KEY_PREV && mode == M_PLAY) {
        open_puzzle(cur - 1); redraw(1);
      } else if (par1 == KEY_NEXT && mode == M_PLAY) {
        open_puzzle(cur + 1); redraw(1);
      }
      break;
    default:
      break;
  }
  return 0;
}

/* ---------- progress file hooks (shell.h) ---------- */

static int keyword_prog_count(void) { return KNUM_PUZZLES; }
static const char *keyword_prog_id(int i) { return KPUZZLES[i].id; }

static int keyword_prog_get(int i, ProgEntry *e) {
  const KwBoard *b = &boards[i];
  int k, n = 0;
  for (k = 0; k < KW_LETTERS; k++)
    if (b->val[k] >= 0) { e->asg_cp[n] = KPUZZLES[i].let[k]; e->asg_digit[n] = (unsigned char)b->val[k]; n++; }
  e->nasg = n;
  e->has_asg = 1;
  e->solved = solved[i];
  e->updated_at = stamp[i];
  return n > 0 || solved[i];
}

/* the incoming assignment; like the source's fromJson, an entry with a letter that is not the puzzle's, a bad digit or
 * the same letter twice is rejected as a whole: fresh board, solved flag ignored */
static void keyword_prog_put(int i, const ProgEntry *e) {
  const KwPuzzle *p = &KPUZZLES[i];
  KwBoard tmp;
  int n, k, bad = e->asg_bad;
  kw_init(&tmp, p);
  for (n = 0; n < e->nasg && !bad; n++) {
    for (k = 0; k < KW_LETTERS; k++) if (p->let[k] == e->asg_cp[n]) break;
    if (k == KW_LETTERS || e->asg_digit[n] > 9 || tmp.val[k] >= 0) bad = 1;
    else kw_set(&tmp, k, e->asg_digit[n]);
  }
  if (bad) kw_reset(&tmp);
  boards[i] = tmp;
  solved[i] = (!bad && e->solved) ? 1 : 0;
  stamp[i] = e->updated_at;
  if (i == cur) { reset_armed = 0; msg[0] = 0; }
}

/* the sample puzzle (index 0) is a tutorial and not counted */
static int keyword_count(void) { return KNUM_PUZZLES - 1; }

static int keyword_solved(void) {
  int i, n = 0;
  for (i = 1; i < KNUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int keyword_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < KNUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (KNUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < KNUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_keyword = {
    "keyword",
    S("Keyword Rebuses", "Ребусы с ключевыми словами", "Schlüsselwort-Rätsel"),
    S("A 3x3 grid of equations hidden behind letters: solve them to reveal a ten-letter keyword.",
      "Сетка 3×3 из примеров, спрятанных за буквами: решите их — и откроется ключевое слово из десяти букв.",
      "Ein 3×3-Gitter voller Aufgaben hinter Buchstaben: Löst sie alle, erscheint ein Schlüsselwort aus zehn Buchstaben."),
    keyword_init,
    keyword_enter,
    keyword_event,
    save_state,
    keyword_count,
    keyword_solved,
    keyword_pick_random,
    "module_keyword_rebus",
    keyword_prog_count,
    keyword_prog_id,
    keyword_prog_get,
    keyword_prog_put,
};
