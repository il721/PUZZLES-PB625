/* Squarewords: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: squareword_core.c / squareword_data.h (port of il721/puzzles, module_squareword).
 * UI: tap a free cell to select it (thick frame), then tap a letter of the keyword below (or beside) the grid to put
 * it there; the eraser empties the cell. A letter that repeats in a row, column or diagonal turns both cells black.
 * The letter buttons turn grey once all n copies of that letter are on the board; the letter in the selected cell is
 * black. Portrait layout only (the reader does not rotate). Progress in /mnt/ext1/squareword.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "../tutorial.h"
#include "squareword_core.h"
#include "squareword_data.h"
#include "squareword_tutorial.h"

#ifndef SQUAREWORDSAVEF
#define SQUAREWORDSAVEF "/mnt/ext1/squareword.sav"
#endif

enum { B_RESET = 10, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE, B_ERASE, B_LETTER = 30 };
enum { M_PLAY, M_LIST, M_HELP, M_GUIDE };

typedef UiBtn Btn;

static SwBoard boards[SNUM_PUZZLES];
static int solved[SNUM_PUZZLES];
static long long stamp[SNUM_PUZZLES]; /* ms since 1970 of the last change to each puzzle, 0 = unknown */
static int cur = 1, mode = M_PLAY;
static int reset_armed, updates;
static char msg[200];
static SwBoard gboard;              /* the walkthrough board, rebuilt for every redraw; never saved */
static uint8_t gemph[SW_MAX][SW_MAX]; /* cells the step has just filled */
static int gstep;                  /* guided walkthrough (mode M_GUIDE): the step, 0-based */
static TutPanel gpanel;
static int left; /* set when the module handed control back to the shell */

static Btn btns[48];
static int nbtn;
static int title_x, title_w;          /* title strip in the top bar */
static int gx, gy, cs;                /* grid origin and cell size */
static int st_x, st_y, st_w, st_h;    /* status area */
static char letter_txt[SW_MAX][4];    /* UTF-8 of the current puzzle's letters, drawn on the palette buttons */

static ifont *f_cell, *f_pal;
static int f_cell_size, f_pal_size;

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_EXAMPLE = S("Example  (1/%d)", "Пример  (1/%d)", "Beispiel  (1/%d)");
static const Str S_PUZZLE = S("Squareword %d/%d%s", "Сквэрворд %d/%d%s", "Quadratwort %d/%d%s");
static const Str S_EX_SHORT = S("Ex", "Пр", "Bsp");
static const Str S_LIST_TITLE = S("Squarewords - choose a puzzle", "Выберите головоломку", "Rätsel wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_FILLED = S("Filled: %d of %d", "Заполнено: %d из %d", "Ausgefüllt: %d von %d");
static const Str S_REPEAT = S("A letter repeats in a row, column or diagonal - shown in black.",
                              "Буква повторяется в строке, столбце или по диагонали — она выделена чёрным.",
                              "Ein Buchstabe wiederholt sich in Zeile, Spalte oder Diagonale — schwarz markiert.");
static const Str S_PICK_CELL = S("Tap an empty cell first.", "Сначала нажмите на пустую клетку.", "Tippen Sie zuerst ein leeres Feld an.");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решено. Меняйте или нажмите «Сбросить».",
                                     "Schon gelöst. Frei ändern oder „Reset“ tippen.");
static const Str S_SOLVED_NEXT = S("Solved! Press > for the next puzzle.", "Решено! Нажмите >, чтобы перейти к следующей головоломке.",
                                   "Gelöst! Mit > geht es zum nächsten Rätsel.");
static const Str S_RESET_AGAIN = S("Tap Reset again to clear all letters.", "Нажмите «Сбросить» ещё раз, чтобы стереть все буквы.",
                                   "Noch einmal „Reset“ tippen, um alle Buchstaben zu löschen.");

static const Str S_HELP_TEXT = S(
    "The top row of the grid is the keyword, and a few more letters are printed (grey cells). Fill the rest so that "
    "each letter of the keyword appears exactly once in every row, every column and on both main diagonals.\n\n"
    "Tap an empty cell, then a letter below the grid to put it there. The eraser button empties the selected cell. "
    "A letter that repeats in a row, column or diagonal turns both cells black; the letter buttons turn grey once all "
    "copies of that letter are on the board. Any complete grid without repeats wins, even if it differs from the "
    "book's. The letters are always Cyrillic: every keyword is a Russian word.\n\n"
    "Puzzle \"Example\" is the sample from the original game. Back key: leave a screen / go home.\n\n"
    "Tap anywhere to close.",
    "Верхняя строка сетки — ключевое слово, ещё несколько букв уже напечатаны (серые клетки). Заполните остальное так, "
    "чтобы каждая буква ключевого слова встречалась ровно один раз в каждой строке, в каждом столбце и на обеих главных "
    "диагоналях.\n\n"
    "Нажмите пустую клетку, затем букву под сеткой — она встанет в клетку. Кнопка «ластик» очищает выбранную клетку. "
    "Буква, повторившаяся в строке, столбце или по диагонали, делает обе клетки чёрными; кнопка буквы становится серой, "
    "когда все её экземпляры уже на доске. Побеждает любая полная сетка без повторов, даже если она отличается от "
    "авторской. Буквы всегда русские: каждое ключевое слово — русское слово.\n\n"
    "Головоломка «Пример» — образец из оригинальной игры. Клавиша «Назад»: закрыть экран / выйти домой.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Die oberste Zeile des Rasters ist das Schlüsselwort, einige weitere Buchstaben sind vorgegeben (graue Felder). "
    "Füllen Sie den Rest so, dass jeder Buchstabe des Schlüsselworts in jeder Zeile, jeder Spalte und auf beiden "
    "Hauptdiagonalen genau einmal vorkommt.\n\n"
    "Tippen Sie ein leeres Feld an, dann einen Buchstaben unter dem Raster. Der Radierer leert das gewählte Feld. "
    "Wiederholt sich ein Buchstabe in Zeile, Spalte oder Diagonale, werden beide Felder schwarz; die Buchstabentaste "
    "wird grau, sobald alle Exemplare auf dem Brett liegen. Jedes vollständige Raster ohne Wiederholung gewinnt, auch "
    "wenn es vom Buch abweicht. Die Buchstaben sind immer kyrillisch: jedes Schlüsselwort ist ein russisches Wort.\n\n"
    "Das Rätsel „Beispiel“ stammt aus dem Originalspiel. Zurück-Taste: Bildschirm verlassen / zum Start.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(SQUAREWORDSAVEF, "w");
  int i;
  char buf[SW_SAVE_LEN + 1];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < SNUM_PUZZLES; i++) {
    sw_save(&boards[i], buf);
    fprintf(f, "%d %s %lld\n", solved[i], buf, stamp[i]);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(SQUAREWORDSAVEF, "r");
  char line[SW_SAVE_LEN + 64];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < SNUM_PUZZLES) cur = v;
  }
  for (i = 0; i < SNUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *sp = strchr(line, ' ');
    char *sp2;
    if (!sp) continue;
    sp2 = strchr(sp + 1, ' ');
    if (sp2) *sp2 = 0;
    solved[i] = atoi(line) ? 1 : 0;
    sw_load(&boards[i], sp + 1);
    stamp[i] = sp2 ? strtoll(sp2 + 1, NULL, 10) : 0;
  }
  fclose(f);
}

/* ---------- fonts ---------- */

static void ensure_cell_font(int size) {
  if (f_cell && size == f_cell_size) return;
  if (f_cell) CloseFont(f_cell);
  f_cell = ui_open_font("LiberationSans-Bold", size);
  f_cell_size = size;
}

static void ensure_pal_font(int size) {
  if (f_pal && size == f_pal_size) return;
  if (f_pal) CloseFont(f_pal);
  f_pal = ui_open_font("LiberationSans-Bold", size);
  f_pal_size = size;
}

/* ---------- layout (portrait) ---------- */

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

/* walkthrough: the grid on top, the step text and its buttons below (tutorial.h) */
static void guide_layout(int W, int H) {
  int i, m = 8, n = gboard.p->n;
  nbtn = 0;
  tut_layout(TUT_SQUARE, gstep, W, H, &gpanel);
  for (i = 0; i < gpanel.nbtn; i++) add_btn(gpanel.btn[i].x, gpanel.btn[i].y, gpanel.btn[i].w, gpanel.btn[i].h, gpanel.btn[i].id, gpanel.btn[i].label);
  gy = 66;
  cs = (gpanel.top - 8 - gy) / n;
  if (cs > (W - 2 * m) / n) cs = (W - 2 * m) / n;
  if (cs > 110) cs = 110;
  gx = (W - n * cs) / 2;
}

static void layout(int W, int H) {
  int m = 8, gap = 6, n = boards[cur].p->n, k;
  int tb = 58;
  int by, bh, bx, bw, ph;
  if (mode == M_GUIDE) { guide_layout(W, H); return; }
  nbtn = 0;

  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;

  gy = tb + 6;
  {
    /* grid on top, then the status, the letter row (n letters + eraser) and the row of buttons */
    int pw;
    bh = (H > 760) ? 64 : 58;
    ph = bh;
    by = H - m - bh;
    st_h = 84;
    cs = (by - gap - ph - gap - st_h - 6 - gy) / n;
    if (cs > (W - 2 * m) / n) cs = (W - 2 * m) / n;
    if (cs > 110) cs = 110;
    gx = (W - n * cs) / 2;
    st_x = m; st_w = W - 2 * m;
    st_y = gy + n * cs + 6;
    st_h = by - gap - ph - gap - st_y;
    pw = (W - 2 * m - n * gap) / (n + 1);
    for (k = 0; k < n; k++) add_btn(m + k * (pw + gap), by - gap - ph, pw, ph, B_LETTER + k, letter_txt[k]);
    add_btn(m + n * (pw + gap), by - gap - ph, W - m - (m + n * (pw + gap)), ph, B_ERASE, "×");
    bx = m; bw = W - 2 * m;
  }
  {
    int w3 = (bw - 2 * gap) / 3;
    add_btn(bx, by, w3, bh, B_PREV, "<");
    add_btn(bx + w3 + gap, by, w3, bh, B_RESET, tr(&S_RESET));
    add_btn(bx + 2 * (w3 + gap), by, w3, bh, B_NEXT, ">");
  }
}

/* ---------- board drawing ---------- */

static void draw_cell(int r, int c, const SwStatus *st) {
  const SwBoard *b = mode == M_GUIDE ? &gboard : &boards[cur];
  int x = gx + c * cs, y = gy + r * cs;
  int given = sw_is_given(b, r, c), bad = st->bad[r][c], sel = (b->sel_r == r && b->sel_c == c);
  int v = b->cell[r][c];
  int fill = bad ? BLACK : given ? 0xDDDDDD : sel ? 0xBBBBBB : WHITE;
  char t[4] = {0, 0, 0, 0};
  int i;
  FillArea(x, y, cs, cs, fill);
  DrawRect(x, y, cs + 1, cs + 1, BLACK);
  if (sel || (mode == M_GUIDE && gemph[r][c])) {
    int th = cs >= 60 ? 5 : 4;
    for (i = 0; i < th; i++) DrawRect(x + i, y + i, cs - 2 * i, cs - 2 * i, bad ? WHITE : BLACK);
  }
  if (v >= 0) memcpy(t, letter_txt[v], 4);
  ui_text_center(f_cell, bad ? WHITE : BLACK, x, y, cs, cs, t);
}

static void draw_board(void) {
  SwStatus st;
  const SwBoard *vb = mode == M_GUIDE ? &gboard : &boards[cur];
  int r, c, n = vb->p->n;
  sw_status(vb, &st);
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++) draw_cell(r, c, &st);
  DrawRect(gx - 1, gy - 1, n * cs + 3, n * cs + 3, BLACK);
  DrawRect(gx - 2, gy - 2, n * cs + 5, n * cs + 5, BLACK);
}

/* the letter buttons and the eraser (drawn before the cells, so a test script's "tapl" hits the button first) */
static void draw_palette_btn(const Btn *b, const SwStatus *st) {
  const SwBoard *bd = &boards[cur];
  int k = b->id - B_LETTER, n = bd->p->n, inverted = 0, grey = 0;
  if (b->id == B_ERASE) { ui_draw_btn(b); return; }
  if (bd->sel_r >= 0 && bd->cell[bd->sel_r][bd->sel_c] == k) inverted = 1;
  else if (st->count[k] >= n) grey = 1;
  ui_box(b->x, b->y, b->w, b->h, 2, inverted ? BLACK : grey ? 0xDDDDDD : WHITE);
  ui_text_center(f_pal, inverted ? WHITE : BLACK, b->x, b->y, b->w, b->h, b->label);
}

/* ---------- screens ---------- */

static void draw_status(const SwStatus *st) {
  char t[300];
  const char *extra = "";
  int n = boards[cur].p->n, len;
  if (reset_armed) extra = tr(&S_RESET_AGAIN);
  else if (msg[0]) extra = msg;
  else if (st->violations) extra = tr(&S_REPEAT);
  len = snprintf(t, sizeof t, tr(&S_FILLED), st->filled, n * n);
  if (extra[0] && len < (int)sizeof t) snprintf(t + len, sizeof t - (size_t)len, "\n%s", extra);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
}

static void draw_play(void) {
  char t[64];
  const SwPuzzle *p = boards[cur].p;
  SwStatus st;
  int i;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), SNUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, SNUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  ui_text_center(ui_f_title, BLACK, title_x, 8, title_w, btns[0].h, t);
  sw_status(&boards[cur], &st);
  for (i = 0; i < nbtn; i++) {
    if (btns[i].id >= B_LETTER || btns[i].id == B_ERASE) draw_palette_btn(&btns[i], &st);
    else ui_draw_btn(&btns[i]);
  }
  draw_board();
  draw_status(&st);
}

static void draw_guide(int W) {
  tut_draw(TUT_SQUARE, gstep, W, &gpanel);
  draw_board();
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 10, cols = 4, rows = (SNUM_PUZZLES + cols - 1) / cols;
  int bw = (W - 2 * m - (cols - 1) * gap) / cols, bh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (bh > bw) bh = bw;
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < SNUM_PUZZLES; i++) {
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
  const SwPuzzle *p = (mode == M_GUIDE ? &gboard : &boards[cur])->p;
  int k;
  for (k = 0; k < p->n; k++) ui_utf8(p->kw[k], letter_txt[k]);
}

static void redraw(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts(28);
  if (mode == M_GUIDE) sq_tut_board(&SPUZZLES[0], gstep, &gboard, gemph);
  if (mode == M_PLAY || mode == M_GUIDE) {
    set_letters();
    layout(W, H);
    ensure_cell_font(cs * 55 / 100 > 18 ? cs * 55 / 100 : 18);
    if (mode == M_PLAY) ensure_pal_font(btns[nbtn - 4].h * 62 / 100);
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
  cur = (i % SNUM_PUZZLES + SNUM_PUZZLES) % SNUM_PUZZLES;
  reset_armed = 0; msg[0] = 0;
  boards[cur].sel_r = boards[cur].sel_c = -1;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  else if (boards[cur].p->tutorial) strcpy(msg, tr(&TUT_S_HINT));
  save_state();
}

/* a tap on a grid cell: select / deselect a free cell; a printed cell only drops the selection */
static void tap_cell(int r, int c) {
  SwBoard *b = &boards[cur];
  reset_armed = 0; msg[0] = 0;
  if (sw_is_given(b, r, c)) { b->sel_r = b->sel_c = -1; return; }
  if (b->sel_r == r && b->sel_c == c) b->sel_r = b->sel_c = -1;
  else { b->sel_r = (int8_t)r; b->sel_c = (int8_t)c; }
}

/* a letter button (letter 0..n-1) or the eraser (-1); returns 1 if the grid is now complete (fresh solve) */
static int put_letter(int letter) {
  SwBoard *b = &boards[cur];
  int won = 0;
  reset_armed = 0; msg[0] = 0;
  if (b->sel_r < 0) { strcpy(msg, tr(&S_PICK_CELL)); return 0; }
  if (b->cell[b->sel_r][b->sel_c] == letter) return 0;
  if (sw_set(b, b->sel_r, b->sel_c, letter)) {
    stamp[cur] = ui_now_ms();
    if (letter >= 0 && sw_wins(b)) { solved[cur] = 1; won = 1; b->sel_r = b->sel_c = -1; strcpy(msg, tr(&S_SOLVED_NEXT)); }
    save_state();
  }
  return won;
}

static void do_button(int id) {
  if (id >= 100 && id < 100 + SNUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; }
      else { sw_reset(&boards[cur]); solved[cur] = 0; stamp[cur] = ui_now_ms(); reset_armed = 0; msg[0] = 0; save_state(); }
      break;
    case B_PREV: open_puzzle(cur - 1); break;
    case B_NEXT: open_puzzle(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
    case TUT_B_GUIDE: mode = M_GUIDE; gstep = 0; break;
    case TUT_B_BACK: case TUT_B_NEXT: case TUT_B_SKIP:
      gstep = tut_next_step(TUT_SQUARE, gstep, id);
      if (gstep < 0) { gstep = 0; mode = M_LIST; }
      break;
    case B_HOME: left = 1; shell_leave(); return;
    default: break;
  }
}

static void tap(int x, int y) {
  int i, n;
  if (mode == M_HELP) { mode = M_PLAY; redraw(1); return; }
  for (i = 0; i < nbtn; i++) {
    const Btn *b = &btns[i];
    if (ui_hit(b, x, y)) {
      int before = mode, won = 0;
      if (mode == M_PLAY && (b->id == B_ERASE || b->id >= B_LETTER) && b->id < 100) won = put_letter(b->id == B_ERASE ? -1 : b->id - B_LETTER);
      else do_button(b->id);
      if (left) { left = 0; return; }
      redraw(won || mode != before || b->id == B_PREV || b->id == B_NEXT || b->id == B_RESET || b->id >= 100);
      return;
    }
  }
  if (mode != M_PLAY) return;
  n = boards[cur].p->n;
  if (x >= gx && x < gx + n * cs && y >= gy && y < gy + n * cs) {
    tap_cell((y - gy) / cs, (x - gx) / cs);
    redraw(0);
  }
}

/* ---------- Module interface (shell.h) ---------- */

static void squareword_init(void) {
  int i;
  for (i = 0; i < SNUM_PUZZLES; i++) sw_init(&boards[i], &SPUZZLES[i]);
  cur = 1;
  load_state();
}

static void squareword_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { reset_armed = 0; msg[0] = 0; boards[cur].sel_r = boards[cur].sel_c = -1; }
  redraw(1);
}

static int squareword_event(int type, int par1, int par2) {
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
        else if (gstep + 1 < TUT_COUNT[TUT_SQUARE]) gstep++;
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

static int squareword_prog_count(void) { return SNUM_PUZZLES; }
static const char *squareword_prog_id(int i) { return SPUZZLES[i].id; }

static int squareword_prog_get(int i, ProgEntry *e) {
  uint8_t l[SW_CELLS][3];
  int n = sw_list(&boards[i], l), k;
  for (k = 0; k < n && k < PROG_MAX_FILL; k++) {
    e->fill[k][0] = l[k][0];
    e->fill[k][1] = l[k][1];
    e->fill_cp[k] = SPUZZLES[i].kw[l[k][2]];
  }
  e->nfill = k;
  e->has_fill = 1;
  e->solved = solved[i];
  e->updated_at = stamp[i];
  return n > 0 || solved[i];
}

/* the incoming letters; cells that are printed, out of range or not letters of the keyword are skipped */
static void squareword_prog_put(int i, const ProgEntry *e) {
  int n, k;
  const SwPuzzle *p = &SPUZZLES[i];
  sw_reset(&boards[i]);
  for (n = 0; n < e->nfill; n++)
    for (k = 0; k < p->n; k++)
      if (p->kw[k] == e->fill_cp[n]) { sw_set(&boards[i], e->fill[n][0], e->fill[n][1], k); break; }
  solved[i] = e->solved ? 1 : 0;
  stamp[i] = e->updated_at;
  if (i == cur) { reset_armed = 0; msg[0] = 0; }
}

/* the sample puzzle (index 0) is a tutorial and not counted */
static int squareword_count(void) { return SNUM_PUZZLES - 1; }

static int squareword_solved(void) {
  int i, n = 0;
  for (i = 1; i < SNUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int squareword_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < SNUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (SNUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < SNUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_squareword = {
    "squareword",
    S("Squarewords", "Сквэрворды", "Quadratwörter"),
    S("Fill the grid so each keyword letter appears once in every row, column and both diagonals.",
      "Заполните сетку так, чтобы каждая буква ключевого слова была один раз в каждой строке, столбце и на обеих диагоналях.",
      "Füllen Sie das Raster so, dass jeder Buchstabe des Schlüsselworts je einmal in jeder Zeile, Spalte und beiden Diagonalen steht."),
    squareword_init,
    squareword_enter,
    squareword_event,
    save_state,
    squareword_count,
    squareword_solved,
    squareword_pick_random,
    "module_squareword",
    squareword_prog_count,
    squareword_prog_id,
    squareword_prog_get,
    squareword_prog_put,
};
