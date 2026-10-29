/* Digit Rebuses: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: digit_core.c / digit_data.h (port of il721/puzzles, module_digit_rebus).
 * UI: tap a cell, tap one of the digits its glyph admits. The source colours the cell borders by glyph; the
 * e-ink screen has no colour, so an empty cell shows the digits its glyph admits instead (small, grey) and the
 * pad offers only those digits. Portrait and landscape layouts. Progress in /mnt/ext1/digit.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "digit_core.h"
#include "digit_data.h"

#ifndef DIGITSAVEF
#define DIGITSAVEF "/mnt/ext1/digit.sav"
#endif

enum { B_DEL = 10, B_CHECK, B_RESET, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE };
enum { M_PLAY, M_LIST, M_HELP };

typedef UiBtn Btn;
typedef struct { int r, c, x, y, w, h; } Cell;

static DGrid grids[DNUM_PUZZLES];
static int solved[DNUM_PUZZLES];
static int cur, mode = M_PLAY;
static int selr = -1, selc = -1;
static DMarks marks;
static int have_marks, reset_armed, updates;
static char msg[200];
static int left; /* set when the module handed control back to the shell */

static Btn btns[32];
static int nbtn;
static Cell cells[16];
static int ncells;
static int title_x, title_y, title_w; /* title strip in the top bar */
static int st_x, st_y, st_w;          /* status line */
static int gx, gy, gw, gh;            /* grid area */

static ifont *f_digit;
static int f_digit_size;

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_CHECK = S("Check", "Проверить", "Prüfen");
static const Str S_DEL = S("Del", "Стереть", "Löschen");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_EXAMPLE = S("Example  (1/%d)", "Пример  (1/%d)", "Beispiel  (1/%d)");
static const Str S_PUZZLE = S("Puzzle %d/%d%s", "Ребус %d/%d%s", "Rätsel %d/%d%s");
static const Str S_EX_SHORT = S("Ex", "Пр", "Bsp");
static const Str S_LIST_TITLE = S("Digit Rebuses - choose a puzzle", "Выберите ребус", "Rätsel wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решён. Меняйте или нажмите «Сбросить».",
                                     "Schon gelöst. Frei ändern oder „Reset“ tippen.");
static const Str S_SOLVED_NEXT = S("Solved! Press > for the next puzzle.", "Решено! Нажмите >, чтобы перейти к следующему ребусу.",
                                   "Gelöst! Mit > geht es zum nächsten Rätsel.");
static const Str S_BROKEN = S("%d rule(s) broken: black cells.", "Нарушено правил: %d (чёрные клетки).",
                              "Verletzte Regeln: %d (schwarze Felder).");
static const Str S_NO_MISTAKES = S("No mistakes so far, %d cell(s) empty.", "Ошибок нет, пустых клеток: %d.",
                                   "Keine Fehler, leere Felder: %d.");
static const Str S_RESET_AGAIN = S("Tap Reset again to clear the grid.", "Нажмите «Сбросить» ещё раз, чтобы очистить квадрат.",
                                   "Noch einmal „Reset“ tippen, um das Quadrat zu leeren.");
static const Str S_HAS_ERRORS = S("The grid is full, but something doesn't add up. Tap Check.",
                                  "Квадрат заполнен, но есть ошибка. Нажмите «Проверить».",
                                  "Das Quadrat ist voll, aber etwas stimmt nicht. „Prüfen“ tippen.");

static const Str S_HELP_TEXT = S(
    "A 4x4 square. Every row and every column is an equation  a op b op c = d, worked strictly from left to right "
    "(no multiplication first); the fourth cell is the result. x multiplies, : divides and must be exact; "
    "no step may go below zero.\n\n"
    "The digits are hidden behind glyphs and each glyph admits only a few digits. An empty cell shows the digits "
    "its glyph admits, and the pad offers only those.\n\n"
    "Tap a cell, then a digit. Check turns the cells of broken rules black. "
    "Any filling where all eight equations hold wins, even if it differs from the book's.\n\n"
    "Puzzle \"Example\" is the sample from the original game. Back key: leave a screen / go home.\n\n"
    "Tap anywhere to close.",
    "Квадрат 4x4. Каждая строка и каждый столбец — пример  a op b op c = d, действия строго слева направо "
    "(умножение не выполняется первым); четвёртая клетка — результат. x умножает, : делит и должно делиться нацело; "
    "ни один шаг не должен уходить ниже нуля.\n\n"
    "Цифры спрятаны за значками, и каждый значок допускает лишь несколько цифр. В пустой клетке показаны цифры, "
    "которые допускает её значок, и на панели предлагаются только они.\n\n"
    "Нажмите клетку, затем цифру. «Проверить» закрашивает клетки нарушенных правил чёрным. "
    "Побеждает любое заполнение, где сходятся все восемь примеров, даже если оно отличается от авторского.\n\n"
    "Ребус «Пример» — образец из оригинальной игры. Клавиша «Назад»: закрыть экран / выйти домой.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Ein 4x4-Quadrat. Jede Zeile und jede Spalte ist eine Rechenaufgabe  a op b op c = d, streng von links nach rechts "
    "gerechnet (keine Punkt-vor-Strich-Regel); das vierte Feld ist das Ergebnis. x multipliziert, : dividiert und muss "
    "aufgehen; kein Schritt darf unter null fallen.\n\n"
    "Die Ziffern sind hinter Symbolen verborgen, und jedes Symbol lässt nur wenige Ziffern zu. Ein leeres Feld zeigt "
    "die Ziffern, die sein Symbol zulässt, und die Leiste bietet nur diese an.\n\n"
    "Feld antippen, dann eine Ziffer. „Prüfen“ färbt die Felder verletzter Regeln schwarz. "
    "Jede Füllung, bei der alle acht Aufgaben stimmen, gewinnt, auch wenn sie von der des Buches abweicht.\n\n"
    "Das Rätsel „Beispiel“ stammt aus dem Originalspiel. Zurück-Taste: Bildschirm verlassen / zum Start.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(DIGITSAVEF, "w");
  int i;
  char buf[DGRID_SAVE_LEN + 1];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < DNUM_PUZZLES; i++) {
    dgrid_save(&grids[i], buf);
    fprintf(f, "%d %s\n", solved[i], buf);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(DIGITSAVEF, "r");
  char line[DGRID_SAVE_LEN + 32];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < DNUM_PUZZLES) cur = v;
  }
  for (i = 0; i < DNUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *sp = strchr(line, ' ');
    if (!sp) continue;
    solved[i] = atoi(line) ? 1 : 0;
    dgrid_load(&grids[i], sp + 1);
  }
  fclose(f);
}

/* ---------- selection ---------- */

static void select_first(void) { selr = selc = 0; }

/* the next empty cell after the selected one in reading order (wrapping); stay if the grid is full */
static void select_next_empty(void) {
  const DGrid *g = &grids[cur];
  int idx = (selr < 0) ? -1 : selr * 4 + selc, n;
  for (n = 1; n <= 16; n++) {
    int j = (idx + n) % 16;
    if (g->d[j / 4][j % 4] < 0) { selr = j / 4; selc = j % 4; return; }
  }
}

/* ---------- fonts ---------- */

static void ensure_digit_font(int size) {
  if (f_digit && size == f_digit_size) return;
  if (f_digit) CloseFont(f_digit);
  f_digit = ui_open_font("LiberationSans-Bold", size);
  f_digit_size = size;
}

/* ---------- layout (portrait or landscape) ---------- */

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

static const char *DIGIT_LABEL[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

/* buttons for the digits the selected cell's glyph admits, left to right from (x, y) in slots of width kw */
static int add_pad_digits(int x, int y, int kw, int gap, int keyh) {
  int d, n = 0;
  if (selr < 0) return 0;
  for (d = 0; d <= 9; d++)
    if (glyph_admits(grids[cur].p->glyph[selr][selc], d)) { add_btn(x + n * (kw + gap), y, kw, keyh, d, DIGIT_LABEL[d]); n++; }
  return n;
}

static void layout(int W, int H) {
  int land = W > H, m = 8, gap = 6;
  int tb = land ? 52 : 58;
  nbtn = 0;

  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;
  title_y = m;

  if (!land) {
    int keyh = (H > 760) ? 84 : 72, kw = (W - 2 * m - 3 * gap) / 4;
    int ky = H - m - 2 * keyh - gap;
    /* pad: the admitted digits (at most three) on the left, Del on the right */
    add_pad_digits(m, ky, kw, gap, keyh);
    add_btn(m + 3 * (kw + gap), ky, kw, keyh, B_DEL, tr(&S_DEL));
    ky += keyh + gap;
    add_btn(m, ky, kw, keyh, B_PREV, "<");
    add_btn(m + (kw + gap), ky, kw, keyh, B_CHECK, tr(&S_CHECK));
    add_btn(m + 2 * (kw + gap), ky, kw, keyh, B_RESET, tr(&S_RESET));
    add_btn(m + 3 * (kw + gap), ky, kw, keyh, B_NEXT, ">");
    st_x = m; st_w = W - 2 * m; st_y = H - m - 2 * keyh - gap - 44;
    gx = m; gy = tb + 4; gw = W - 2 * m; gh = st_y - gy;
  } else {
    int kpw = 300, kw = (kpw - 2 * gap) / 3, kx = W - m - kpw;
    int keyh = (H - tb - 2 * m - 2 * gap) / 3, ky = tb + m;
    int hw = (kpw - gap) / 2, aw = (kx - m - gap - 2 * gap) / 3, ah = 62, ay = H - m - ah;
    add_pad_digits(kx, ky, kw, gap, keyh);
    add_btn(kx, ky + keyh + gap, hw, keyh, B_DEL, tr(&S_DEL));
    add_btn(kx + hw + gap, ky + keyh + gap, hw, keyh, B_CHECK, tr(&S_CHECK));
    add_btn(m, ay, aw, ah, B_PREV, "<");
    add_btn(m + aw + gap, ay, aw, ah, B_RESET, tr(&S_RESET));
    add_btn(m + 2 * (aw + gap), ay, aw, ah, B_NEXT, ">");
    st_x = m; st_w = kx - m - gap; st_y = ay - 44;
    gx = m; gy = tb + 4; gw = kx - m - gap; gh = st_y - gy;
  }
}

/* ---------- grid drawing ---------- */

static void build_and_draw_grid(int draw) {
  const DPuzzle *p = grids[cur].p;
  const DGrid *g = &grids[cur];
  int r, c, bs, lane, total, x0, y0;
  int xcol[4], yrow[4];

  /* 4 cells + 3 operator lanes (half a cell wide) in each direction */
  bs = gw * 2 / 11;
  if (gh * 2 / 11 < bs) bs = gh * 2 / 11;
  if (bs > 110) bs = 110;
  lane = bs / 2;
  total = 4 * bs + 3 * lane;
  x0 = gx + (gw - total) / 2;
  y0 = gy + (gh - total) / 2;
  for (c = 0; c < 4; c++) { xcol[c] = x0 + c * (bs + lane); yrow[c] = y0 + c * (bs + lane); }

  ncells = 0;
  for (r = 0; r < 4; r++)
    for (c = 0; c < 4; c++) {
      Cell *e = &cells[ncells++];
      e->r = r; e->c = c; e->x = xcol[c]; e->y = yrow[r]; e->w = bs; e->h = bs;
    }
  if (!draw) return;

  /* operators: along the rows between the cells, down the columns between the rows; the last one is "=" */
  for (r = 0; r < 4; r++)
    for (c = 0; c < 3; c++) {
      char o[2] = {0, 0};
      const char *t = "=";
      if (c < 2) { o[0] = (p->rowops[r][c] == '*') ? 'x' : p->rowops[r][c]; t = o; }
      ui_text_center(ui_f_btn, BLACK, xcol[c] + bs, yrow[r], lane, bs, t);
    }
  for (c = 0; c < 4; c++)
    for (r = 0; r < 3; r++) {
      char o[2] = {0, 0};
      const char *t = "=";
      if (r < 2) { o[0] = (p->colops[c][r] == '*') ? 'x' : p->colops[c][r]; t = o; }
      ui_text_center(ui_f_btn, BLACK, xcol[c], yrow[r] + bs, bs, lane, t);
    }

  for (r = 0; r < 4; r++)
    for (c = 0; c < 4; c++) {
      int d = g->d[r][c];
      int bad = have_marks && marks.bad[r][c];
      int issel = (r == selr && c == selc);
      char t[2] = {0, 0};
      ui_box(xcol[c], yrow[r], bs, bs, issel ? 5 : 2, bad ? BLACK : WHITE);
      if (d >= 0) {
        t[0] = (char)('0' + d);
        ui_text_center(f_digit, bad ? WHITE : BLACK, xcol[c], yrow[r], bs, bs, t);
      } else {
        char hint[16];
        int k, n = 0;
        for (k = 0; k <= 9; k++)
          if (glyph_admits(p->glyph[r][c], k)) { if (n) hint[n++] = ' '; hint[n++] = (char)('0' + k); }
        hint[n] = 0;
        ui_text_center(ui_f_small, 0x555555, xcol[c], yrow[r], bs, bs, hint);
      }
    }
}

/* ---------- screens ---------- */

static void draw_play(int W, int H) {
  char t[64];
  const DPuzzle *p = grids[cur].p;
  int i;
  (void)H; (void)W;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), DNUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, DNUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  ui_text_center(ui_f_title, BLACK, title_x, title_y, title_w, btns[0].h, t);
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);
  build_and_draw_grid(1);
  if (msg[0]) {
    SetFont(ui_f_small, BLACK);
    DrawTextRect(st_x, st_y, st_w, 40, msg, ALIGN_CENTER | VALIGN_MIDDLE);
  }
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 10, cols = 4, rows = (DNUM_PUZZLES + cols - 1) / cols;
  int bw = (W - 2 * m - (cols - 1) * gap) / cols, bh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (bh > bw) bh = bw;
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < DNUM_PUZZLES; i++) {
    int x = m + (i % cols) * (bw + gap), y = top + (i / cols) * (bh + gap);
    char t[24];
    if (grids[i].p->tutorial) snprintf(t, sizeof t, "%s%s", tr(&S_EX_SHORT), solved[i] ? " ok" : "");
    else snprintf(t, sizeof t, "%d%s", i, solved[i] ? " ok" : "");
    ui_box(x, y, bw, bh, i == cur ? 4 : 2, solved[i] ? 0xDDDDDD : WHITE);
    ui_text_center(ui_f_btn, BLACK, x, y, bw, bh, t);
    add_btn(x, y, bw, bh, 100 + i, "");
  }
  {
    int bwid = 200, bx = (W - 2 * bwid - 20) / 2, by = H - 80; /* Back (to the puzzle) and Home (to the main menu) */
    add_btn(bx, by, bwid, 64, B_CLOSE, tr(&S_BACK));
    add_btn(bx + bwid + 20, by, bwid, 64, B_HOME, tr(&S_HOME));
    ui_draw_btn(&btns[nbtn - 2]);
    ui_draw_btn(&btns[nbtn - 1]);
  }
}

static void draw_help(int W, int H) {
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_HELP_TITLE));
  SetFont(ui_f_small, BLACK);
  DrawTextRect(16, 64, W - 32, H - 80, (char *)tr(&S_HELP_TEXT), ALIGN_LEFT);
}

static void redraw(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  int dsize = ((W > H) ? H : W) / 11;
  if (dsize < 22) dsize = 22;
  ensure_digit_font(dsize);
  ui_fonts((W > H) ? 26 : 28);
  if (mode == M_PLAY) layout(W, H);
  ClearScreen();
  if (mode == M_PLAY) draw_play(W, H);
  else if (mode == M_LIST) draw_list(W, H);
  else draw_help(W, H);
  updates++;
  if (full || updates % 8 == 0) FullUpdate();
  else PartialUpdate(0, 0, W, H);
}

/* ---------- actions ---------- */

static void open_puzzle(int i) {
  cur = (i % DNUM_PUZZLES + DNUM_PUZZLES) % DNUM_PUZZLES;
  have_marks = 0; reset_armed = 0; msg[0] = 0;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  select_first();
  save_state();
}

static void enter_digit(int d) {
  DGrid *g = &grids[cur];
  if (selr < 0 || !glyph_admits(g->p->glyph[selr][selc], d) || !dgrid_set(g, selr, selc, d)) return;
  have_marks = 0; reset_armed = 0; msg[0] = 0;
  if (dgrid_wins(g)) {
    solved[cur] = 1;
    strcpy(msg, tr(&S_SOLVED_NEXT));
  } else {
    if (dgrid_complete(g)) strcpy(msg, tr(&S_HAS_ERRORS));
    select_next_empty();
  }
  save_state();
}

static void do_check(void) {
  DGrid *g = &grids[cur];
  int r, c, empty = 0;
  reset_armed = 0;
  have_marks = 1;
  dgrid_violations(g, &marks);
  for (r = 0; r < 4; r++)
    for (c = 0; c < 4; c++) empty += g->d[r][c] < 0;
  if (dgrid_wins(g)) { solved[cur] = 1; strcpy(msg, tr(&S_SOLVED_NEXT)); save_state(); }
  else if (marks.count) snprintf(msg, sizeof msg, tr(&S_BROKEN), marks.count);
  else snprintf(msg, sizeof msg, tr(&S_NO_MISTAKES), empty);
}

static void do_button(int id) {
  DGrid *g = &grids[cur];
  if (id >= 100 && id < 100 + DNUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_DEL:
      if (selr >= 0 && dgrid_clear(g, selr, selc)) { have_marks = 0; msg[0] = 0; save_state(); }
      break;
    case B_CHECK: do_check(); break;
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; strcpy(msg, tr(&S_RESET_AGAIN)); }
      else { dgrid_reset(g); solved[cur] = 0; have_marks = 0; reset_armed = 0; msg[0] = 0; select_first(); save_state(); }
      break;
    case B_PREV: open_puzzle(cur - 1); break;
    case B_NEXT: open_puzzle(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
    case B_HOME: left = 1; shell_leave(); return;
    default:
      if (id >= 0 && id <= 9) enter_digit(id);
  }
}

static void tap(int x, int y) {
  int i;
  if (mode == M_HELP) { mode = M_PLAY; redraw(1); return; }
  for (i = 0; i < nbtn; i++) {
    const Btn *b = &btns[i];
    if (ui_hit(b, x, y)) {
      int before = mode;
      do_button(b->id);
      if (left) { left = 0; return; }
      redraw(mode != before || b->id == B_PREV || b->id == B_NEXT || b->id == B_RESET || b->id >= 100);
      return;
    }
  }
  if (mode != M_PLAY) return;
  for (i = 0; i < ncells; i++) {
    const Cell *c = &cells[i];
    if (x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h) {
      selr = c->r; selc = c->c; reset_armed = 0;
      redraw(0);
      return;
    }
  }
}

/* ---------- Module interface (shell.h) ---------- */

static void digit_init(void) {
  int i;
  for (i = 0; i < DNUM_PUZZLES; i++) dgrid_init(&grids[i], &DPUZZLES[i]);
  cur = 1;
  load_state();
  select_first();
}

static void digit_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { have_marks = 0; reset_armed = 0; msg[0] = 0; if (selr < 0) select_first(); }
  redraw(1);
}

static int digit_event(int type, int par1, int par2) {
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
        if (mode != M_PLAY) { mode = M_PLAY; redraw(1); }
        else shell_leave();
      } else if (par1 == KEY_HOME || par1 == KEY_MENU) {
        shell_leave(); /* the reader's hardware button: straight back to the home screen */
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

/* the sample puzzle (index 0) is a tutorial and not counted */
static int digit_count(void) { return DNUM_PUZZLES - 1; }

static int digit_solved(void) {
  int i, n = 0;
  for (i = 1; i < DNUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int digit_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < DNUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (DNUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < DNUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_digit = {
    "digit",
    S("Digit Rebuses", "Цифровые ребусы", "Ziffernrebusse"),
    S("Every row and every column is an equation: place digits following the glyph hints.",
      "Каждая строка и каждый столбец — пример: расставьте цифры по значкам-подсказкам.",
      "Jede Zeile und jede Spalte ist eine Rechenaufgabe: Setzen Sie die Ziffern anhand der Symbolhinweise."),
    digit_init,
    digit_enter,
    digit_event,
    save_state,
    digit_count,
    digit_solved,
    digit_pick_random,
};
