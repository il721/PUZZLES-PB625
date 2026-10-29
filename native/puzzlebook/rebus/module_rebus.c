/* Square Rebuses: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: rebus_core.c / rebus_data.h (port of il721/puzzles, module_rebus).
 * UI: tap a box, tap a digit. Portrait layout only (the reader does not rotate). Progress in /mnt/ext1/rebus.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "rebus_core.h"
#include "rebus_data.h"

#ifndef SAVEF
#define SAVEF "/mnt/ext1/rebus.sav"
#endif

enum { B_D0 = 0, B_DEL = 10, B_CHECK, B_RESET, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE };
enum { M_PLAY, M_LIST, M_HELP };

typedef UiBtn Btn;
typedef struct { int r, s, k, x, y, w, h; } Cell;

static Grid grids[NUM_PUZZLES];
static int solved[NUM_PUZZLES];
static long long stamp[NUM_PUZZLES]; /* ms since 1970 of the last change to each puzzle, 0 = unknown */
static int cur, mode = M_PLAY;
static int selr = -1, sels = -1, selk = -1;
static Marks marks;
static int have_marks, reset_armed, updates;
static char msg[200];
static int left; /* set when the module handed control back to the shell */

static Btn btns[32];
static int nbtn;
static Cell cells[75];
static int ncells;
static int title_x, title_y, title_w; /* title strip in the top bar */
static int st_x, st_y, st_w;          /* status line */

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
static const Str S_LIST_TITLE = S("Square Rebuses - choose a puzzle", "Выберите ребус", "Rätsel wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решён. Меняйте или нажмите «Сбросить».",
                                     "Schon gelöst. Frei ändern oder „Reset“ tippen.");
static const Str S_SOLVED_NEXT = S("Solved! Press > for the next puzzle.", "Решено! Нажмите >, чтобы перейти к следующему ребусу.",
                                   "Gelöst! Mit > geht es zum nächsten Rätsel.");
static const Str S_BROKEN = S("%d rule(s) broken: black boxes.", "Нарушено правил: %d (чёрные клетки).",
                              "Verletzte Regeln: %d (schwarze Felder).");
static const Str S_NO_MISTAKES = S("No mistakes so far, %d box(es) empty.", "Ошибок нет, пустых клеток: %d.",
                                   "Keine Fehler, leere Felder: %d.");
static const Str S_RESET_AGAIN = S("Tap Reset again to clear the grid.", "Нажмите «Сбросить» ещё раз, чтобы очистить квадрат.",
                                   "Noch einmal „Reset“ tippen, um das Quadrat zu leeren.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(SAVEF, "w");
  int i;
  char buf[GRID_SAVE_LEN + 1];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < NUM_PUZZLES; i++) {
    grid_save(&grids[i], buf);
    fprintf(f, "%d %s %lld\n", solved[i], buf, stamp[i]);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(SAVEF, "r");
  char line[GRID_SAVE_LEN + 32];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < NUM_PUZZLES) cur = v;
  }
  for (i = 0; i < NUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *sp = strchr(line, ' ');
    if (!sp) continue;
    char *sp2 = strchr(sp + 1, ' '); /* optional stamp after the grid; older saves have none */
    solved[i] = atoi(line) ? 1 : 0;
    grid_load(&grids[i], sp + 1);
    stamp[i] = sp2 ? strtoll(sp2 + 1, NULL, 10) : 0;
  }
  fclose(f);
}

/* ---------- selection ---------- */

static int editable(const Grid *g, int r, int s, int k) { return k < g->width[r][s] && !g->given[r][s][k]; }

static void select_first(void) {
  int r, s, k;
  const Grid *g = &grids[cur];
  selr = sels = selk = -1;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++)
      for (k = 0; k < MAXPOS; k++)
        if (editable(g, r, s, k)) { selr = r; sels = s; selk = k; return; }
}

static void select_next(void) {
  const Grid *g = &grids[cur];
  int idx = (selr < 0) ? -1 : selr * 15 + sels * 3 + selk, n;
  for (n = 1; n <= 75; n++) {
    int j = (idx + n) % 75, r = j / 15, s = (j / 3) % 5, k = j % 3;
    if (editable(g, r, s, k)) { selr = r; sels = s; selk = k; return; }
  }
}

/* ---------- fonts ---------- */

static void ensure_digit_font(int size) {
  if (f_digit && size == f_digit_size) return;
  if (f_digit) CloseFont(f_digit);
  f_digit = ui_open_font("LiberationSans-Bold", size);
  f_digit_size = size;
}

/* ---------- drawing helpers ---------- */

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

static const char *DIGIT_LABEL[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

/* ---------- layout (portrait) ---------- */

static int gx, gy, gw, gh; /* grid area */

static void layout(int W, int H) {
  int m = 8, gap = 6, i;
  int tb = 58;
  nbtn = 0;

  /* top bar */
  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;
  title_y = m;

  {
    int keyh = (H > 760) ? 84 : 72, kw = (W - 2 * m - 4 * gap) / 5;
    int ky = H - m - 3 * keyh - 2 * gap;
    for (i = 0; i < 5; i++) add_btn(m + i * (kw + gap), ky, kw, keyh, 1 + i, DIGIT_LABEL[1 + i]);
    for (i = 0; i < 4; i++) add_btn(m + i * (kw + gap), ky + keyh + gap, kw, keyh, 6 + i, DIGIT_LABEL[6 + i]);
    add_btn(m + 4 * (kw + gap), ky + keyh + gap, kw, keyh, B_D0, "0");
    ky += 2 * (keyh + gap);
    add_btn(m, ky, kw, keyh, B_PREV, "<");
    add_btn(m + (kw + gap), ky, kw, keyh, B_CHECK, tr(&S_CHECK));
    add_btn(m + 2 * (kw + gap), ky, kw, keyh, B_DEL, tr(&S_DEL));
    add_btn(m + 3 * (kw + gap), ky, kw, keyh, B_RESET, tr(&S_RESET));
    add_btn(m + 4 * (kw + gap), ky, kw, keyh, B_NEXT, ">");
    st_x = m; st_w = W - 2 * m; st_y = H - m - 3 * keyh - 2 * gap - 44;
    gx = m; gy = tb + 4; gw = W - 2 * m; gh = st_y - gy;
  }
}

/* ---------- grid drawing ---------- */

static void build_and_draw_grid(int draw) {
  const Puzzle *p = grids[cur].p;
  const Grid *g = &grids[cur];
  int colw[5], j, r, s, k;
  double units;
  int bh, bw, opw, pitch, divgap, totalw, totalh, x0, y0;
  int xcol[5], opx[5];

  for (j = 0; j < 4; j++) {
    colw[j] = g->width[4][j];
    for (r = 0; r < 4; r++)
      if (g->width[r][j] > colw[j]) colw[j] = g->width[r][j];
  }
  colw[4] = g->width[4][4];
  for (r = 0; r < 4; r++)
    if (g->width[r][4] > colw[4]) colw[4] = g->width[r][4];

  units = colw[0] + colw[1] + colw[2] + colw[3] + colw[4] + 4 * 0.8;
  bw = (int)(gw / units);
  bh = (int)(gh / (5 * 1.12 + 0.45));
  if (bh > (int)(bw * 1.35)) bh = (int)(bw * 1.35);
  if (bw > (int)(bh * 0.95)) bw = (int)(bh * 0.95);
  if (bw > 72) bw = 72;
  opw = (int)(bw * 0.8);
  pitch = (int)(bh * 1.12);
  divgap = (int)(bh * 0.45);
  totalw = (int)(bw * (colw[0] + colw[1] + colw[2] + colw[3] + colw[4])) + 4 * opw;
  totalh = 5 * pitch + divgap;
  x0 = gx + (gw - totalw) / 2;
  y0 = gy + (gh - totalh) / 2;

  {
    int x = x0;
    for (j = 0; j < 5; j++) {
      xcol[j] = x;
      x += colw[j] * bw;
      if (j < 4) { opx[j] = x; x += opw; }
    }
  }

  ncells = 0;
  for (r = 0; r < 5; r++) {
    int y = y0 + r * pitch + (r == 4 ? divgap : 0);
    for (s = 0; s < 5; s++) {
      int w = g->width[r][s], xs = xcol[s] + (colw[s] - w) * bw;
      for (k = 0; k < w; k++) {
        Cell *c = &cells[ncells++];
        c->r = r; c->s = s; c->k = k; c->x = xs + k * bw; c->y = y; c->w = bw; c->h = bh;
      }
    }
    if (draw) {
      char o[2] = {0, 0};
      for (j = 0; j < 4; j++) {
        const char *t;
        if (r < 4) {
          char c = p->rows[r].ops[j < 3 ? j : 0];
          o[0] = (c == '*') ? 'x' : c;
          t = (j < 3) ? o : "=";
        } else {
          t = (j < 3) ? "+" : "=";
        }
        ui_text_center(ui_f_btn, BLACK, opx[j], y, opw, bh, t);
      }
    }
  }
  if (!draw) return;

  {
    int ly = y0 + 3 * pitch + bh + (pitch - bh + divgap) / 2; /* between row 3 and the summary row */
    DrawLine(x0, ly, x0 + totalw, ly, BLACK);
    DrawLine(x0, ly + 1, x0 + totalw, ly + 1, BLACK);
  }

  for (j = 0; j < ncells; j++) {
    const Cell *c = &cells[j];
    int given = g->given[c->r][c->s][c->k], d = g->digit[c->r][c->s][c->k];
    int bad = have_marks && marks.bad[c->r][c->s][c->k];
    int issel = (c->r == selr && c->s == sels && c->k == selk);
    int fill = bad ? BLACK : (given ? 0xCCCCCC : WHITE);
    char t[2] = {0, 0};
    ui_box(c->x + 1, c->y, c->w - 2, c->h, issel ? 4 : 1, fill);
    if (d >= 0) {
      t[0] = (char)('0' + d);
      ui_text_center(f_digit, bad ? WHITE : BLACK, c->x, c->y, c->w, c->h, t);
    }
  }
}

/* ---------- screens ---------- */

static void draw_buttons(void) {
  int i;
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);
}

static const Str S_HELP_TEXT = S(
    "Each row is  a op b op c op d = result, worked strictly from left to right "
    "(no multiplication first).  x multiplies, : divides and must be exact.\n\n"
    "All numbers are whole, at least 1, without a leading zero. Every box is one digit.\n\n"
    "Column 1 (the first number of each row, added up) must equal the result of row 1; "
    "column 2 the result of row 2, and so on.\n\n"
    "The bottom row repeats the four results and shows their total.\n\n"
    "Grey boxes are given. Tap a box, then a digit. Check turns broken rules black.\n\n"
    "Puzzle \"Example\" is the sample from the original game. Back key: leave a screen / go home.\n\n"
    "Tap anywhere to close.",
    "Каждая строка: a op b op c op d = результат, действия строго слева направо "
    "(умножение не выполняется первым).  x умножает, : делит и должно делиться нацело.\n\n"
    "Все числа целые, не меньше 1, без ведущего нуля. В каждой клетке одна цифра.\n\n"
    "Сумма первых чисел всех строк (столбец 1) равна результату строки 1; "
    "столбец 2 — результату строки 2 и так далее.\n\n"
    "Нижняя строка повторяет четыре результата и показывает их общий итог.\n\n"
    "Серые клетки заданы. Нажмите клетку, затем цифру. «Проверить» закрашивает нарушенные правила чёрным.\n\n"
    "Ребус «Пример» — образец из оригинальной игры. Клавиша «Назад»: закрыть экран / выйти домой.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Jede Zeile lautet  a op b op c op d = Ergebnis und wird strikt von links nach rechts gerechnet "
    "(keine Punkt-vor-Strich-Regel).  x multipliziert, : dividiert und muss aufgehen.\n\n"
    "Alle Zahlen sind ganz, mindestens 1 und ohne führende Null. Jedes Kästchen ist eine Ziffer.\n\n"
    "Spalte 1 (die ersten Zahlen aller Zeilen, addiert) muss das Ergebnis von Zeile 1 ergeben; "
    "Spalte 2 das Ergebnis von Zeile 2 und so weiter.\n\n"
    "Die untere Zeile wiederholt die vier Ergebnisse und zeigt ihre Summe.\n\n"
    "Graue Kästchen sind vorgegeben. Kästchen antippen, dann eine Ziffer. „Prüfen“ färbt verletzte Regeln schwarz; „Reset“ leert das Quadrat.\n\n"
    "Das Rätsel „Beispiel“ stammt aus dem Originalspiel. Zurück-Taste: Bildschirm verlassen / zum Start.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

static void draw_play(int W, int H) {
  char t[64];
  const Puzzle *p = grids[cur].p;
  (void)H;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), NUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, NUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  ui_text_center(ui_f_title, BLACK, title_x, title_y, title_w, btns[0].h, t);
  draw_buttons();
  build_and_draw_grid(1);
  if (msg[0]) {
    SetFont(ui_f_small, BLACK);
    DrawTextRect(st_x, st_y, st_w, 40, msg, ALIGN_CENTER | VALIGN_MIDDLE);
  }
  (void)W;
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 10, cols = 4, rows = (NUM_PUZZLES + cols - 1) / cols;
  int bw = (W - 2 * m - (cols - 1) * gap) / cols, bh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (bh > bw) bh = bw;
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < NUM_PUZZLES; i++) {
    int x = m + (i % cols) * (bw + gap), y = top + (i / cols) * (bh + gap);
    char t[16];
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
  int dsize = W / 11;
  if (dsize < 22) dsize = 22;
  ensure_digit_font(dsize);
  ui_fonts(28);
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
  cur = (i % NUM_PUZZLES + NUM_PUZZLES) % NUM_PUZZLES;
  have_marks = 0; reset_armed = 0; msg[0] = 0;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  select_first();
  save_state();
}

static void enter_digit(int d) {
  Grid *g = &grids[cur];
  if (selr < 0 || !grid_set(g, selr, sels, selk, d)) return;
  have_marks = 0; reset_armed = 0; msg[0] = 0;
  stamp[cur] = ui_now_ms();
  if (grid_wins(g)) {
    solved[cur] = 1;
    strcpy(msg, tr(&S_SOLVED_NEXT));
  } else {
    select_next();
  }
  save_state();
}

static void do_check(void) {
  Grid *g = &grids[cur];
  int i, empty = 0;
  reset_armed = 0;
  have_marks = 1;
  grid_violations(g, &marks);
  for (i = 0; i < 75; i++) if (g->digit[i / 15][(i / 3) % 5][i % 3] < 0 && (i % 3) < g->width[i / 15][(i / 3) % 5]) empty++;
  if (grid_wins(g)) { solved[cur] = 1; stamp[cur] = ui_now_ms(); strcpy(msg, tr(&S_SOLVED_NEXT)); save_state(); }
  else if (marks.count) snprintf(msg, sizeof msg, tr(&S_BROKEN), marks.count);
  else snprintf(msg, sizeof msg, tr(&S_NO_MISTAKES), empty);
}

static void do_button(int id) {
  Grid *g = &grids[cur];
  if (id >= 100 && id < 100 + NUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_DEL:
      if (selr >= 0 && grid_clear(g, selr, sels, selk)) { have_marks = 0; msg[0] = 0; stamp[cur] = ui_now_ms(); save_state(); }
      break;
    case B_CHECK: do_check(); break;
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; strcpy(msg, tr(&S_RESET_AGAIN)); }
      else { grid_reset(g); solved[cur] = 0; have_marks = 0; reset_armed = 0; msg[0] = 0; select_first(); save_state(); }
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
    if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) {
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
    if (x >= c->x && x < c->x + c->w && y >= c->y - 4 && y < c->y + c->h + 4) {
      if (editable(&grids[cur], c->r, c->s, c->k)) { selr = c->r; sels = c->s; selk = c->k; reset_armed = 0; }
      redraw(0);
      return;
    }
  }
}

/* ---------- Module interface (shell.h) ---------- */

static void rebus_init(void) {
  int i;
  for (i = 0; i < NUM_PUZZLES; i++) grid_init(&grids[i], &PUZZLES[i]);
  cur = 1;
  load_state();
  select_first();
}

static void rebus_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { have_marks = 0; reset_armed = 0; msg[0] = 0; if (selr < 0) select_first(); }
  redraw(1);
}

static int rebus_event(int type, int par1, int par2) {
  (void)par2;
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
        /* the reader's hardware button: straight back to the home screen from any screen of the module */
        shell_leave();
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

static int rebus_prog_count(void) { return NUM_PUZZLES; }
static const char *rebus_prog_id(int i) { return PUZZLES[i].id; }

static int rebus_prog_get(int i, ProgEntry *e) {
  const Grid *g = &grids[i];
  int r, s, k, n = 0;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++)
      for (k = 0; k < g->width[r][s]; k++)
        if (!g->given[r][s][k] && g->digit[r][s][k] >= 0 && n < PROG_MAX_CELLS) {
          snprintf(e->cells[n].key, sizeof e->cells[n].key, "%d:%d:%d", r, s, k);
          e->cells[n++].digit = g->digit[r][s][k];
        }
  e->ncells = n;
  e->solved = solved[i];
  e->updated_at = stamp[i];
  return n > 0 || solved[i];
}

/* "row:slot:pos" -> grid_set; keys that name no editable cell are ignored */
static void rebus_prog_put(int i, const ProgEntry *e) {
  Grid *g = &grids[i];
  int n;
  grid_reset(g);
  for (n = 0; n < e->ncells; n++) {
    const char *k = e->cells[n].key;
    char *end;
    long r = strtol(k, &end, 10), s, p;
    if (*end != ':') continue;
    s = strtol(end + 1, &end, 10);
    if (*end != ':') continue;
    p = strtol(end + 1, &end, 10);
    if (*end) continue;
    grid_set(g, (int)r, (int)s, (int)p, e->cells[n].digit);
  }
  solved[i] = e->solved ? 1 : 0;
  stamp[i] = e->updated_at;
  if (i == cur) { have_marks = 0; reset_armed = 0; msg[0] = 0; select_first(); }
}

/* the sample puzzle (index 0) is a tutorial and not counted */
static int rebus_count(void) { return NUM_PUZZLES - 1; }

static int rebus_solved(void) {
  int i, n = 0;
  for (i = 1; i < NUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int rebus_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < NUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (NUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < NUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_rebus = {
    "rebus",
    S("Square Rebuses", "Ребусы с квадратиками", "Zahlenrätsel-Quadrate"),
    S("Arithmetic rebuses: fill in the boxes so every equation, column sum and the total check out.",
      "Арифметические ребусы: заполните квадратики цифрами, чтобы сошлись все уравнения, суммы столбцов и итог.",
      "Rechenrätsel: Füllen Sie die Kästchen so aus, dass jede Gleichung, jede Spaltensumme und das Ergebnis aufgeht."),
    rebus_init,
    rebus_enter,
    rebus_event,
    save_state,
    rebus_count,
    rebus_solved,
    rebus_pick_random,
    "module_rebus",
    rebus_prog_count,
    rebus_prog_id,
    rebus_prog_get,
    rebus_prog_put,
};
