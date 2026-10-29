/* Domino Solitaire: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: domino_core.c / domino_data.h (port of il721/puzzles, module_domino).
 * UI: tap a cell, then an adjacent free cell: the two become a domino (one tile, no seam). Tap a placed domino to
 * take it back. On the right the 28 value pairs of the double-six set: pairs already on the board are black. A pair
 * used twice turns both of its dominoes black (the source tints them red). Portrait and landscape layouts.
 * Progress in /mnt/ext1/domino.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "../tutorial.h"
#include "domino_core.h"
#include "domino_data.h"
#include "domino_tutorial.h"

#ifndef DOMINOSAVEF
#define DOMINOSAVEF "/mnt/ext1/domino.sav"
#endif

enum { B_RESET = 10, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE };
enum { M_PLAY, M_LIST, M_HELP, M_GUIDE };

typedef UiBtn Btn;

static DomBoard boards[DNUM_PUZZLES];
static int solved[DNUM_PUZZLES];
static long long stamp[DNUM_PUZZLES]; /* ms since 1970 of the last change to each puzzle, 0 = unknown */
static int cur = 1, mode = M_PLAY;
static int reset_armed, updates;
static char msg[200];
static DomBoard gboard;             /* the walkthrough board, rebuilt for every redraw; never saved */
static uint8_t gemph[DCELLS];       /* cells of the dominoes the step has just placed */
static int gstep;                  /* guided walkthrough (mode M_GUIDE): the step, 0-based */
static TutPanel gpanel;
static int left; /* set when the module handed control back to the shell */

static Btn btns[32];
static int nbtn;
static int title_x, title_w;          /* title strip in the top bar */
static int gx, gy, cs;                /* grid origin and cell size */
static int chip_x, chip_y, chip_w, chip_h, chip_cols, chip_gap; /* the 28 value pairs */
static int st_x, st_y, st_w, st_h;    /* status area */

static ifont *f_digit, *f_chip;
static int f_digit_size, f_chip_size;

/* chip k is the pair (CHIP_LO[k], CHIP_HI[k]) in ascending order 0:0, 0:1 .. 6:6 */
static int CHIP_LO[DDOMS], CHIP_HI[DDOMS];
static char CHIP_TXT[DDOMS][4];

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_EXAMPLE = S("Example  (1/%d)", "Пример  (1/%d)", "Beispiel  (1/%d)");
static const Str S_PUZZLE = S("Domino %d/%d%s", "Домино %d/%d%s", "Domino %d/%d%s");
static const Str S_EX_SHORT = S("Ex", "Пр", "Bsp");
static const Str S_LIST_TITLE = S("Domino Solitaire - choose a puzzle", "Выберите головоломку", "Rätsel wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_PLACED = S("Placed: %d of 28", "Выставлено: %d из 28", "Gelegt: %d von 28");
static const Str S_DUPLICATE = S("Some dominoes are duplicated - shown in black.", "Есть повторяющиеся косточки — они выделены чёрным.",
                                 "Einige Dominosteine sind doppelt — schwarz markiert.");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решено. Меняйте или нажмите «Сбросить».",
                                     "Schon gelöst. Frei ändern oder „Reset“ tippen.");
static const Str S_SOLVED_NEXT = S("Solved! Press > for the next puzzle.", "Решено! Нажмите >, чтобы перейти к следующей головоломке.",
                                   "Gelöst! Mit > geht es zum nächsten Rätsel.");
static const Str S_RESET_AGAIN = S("Tap Reset again to remove all dominoes.", "Нажмите «Сбросить» ещё раз, чтобы убрать все косточки.",
                                   "Noch einmal „Reset“ tippen, um alle Steine zu entfernen.");

static const Str S_HELP_TEXT = S(
    "The board holds 56 cells with the digits 0 to 6. Split them into 28 dominoes so that every value pair from 0:0 "
    "to 6:6 appears exactly once - the full set.\n\n"
    "Tap a cell, then an adjacent cell: the two become a domino. Tap a finished domino to take it back. "
    "On the right the 28 pairs are listed; pairs already on the board are black. If the same pair appears "
    "twice, both of its dominoes turn black. Any complete tiling with all 28 pairs wins, even if it differs "
    "from the book's.\n\n"
    "Puzzle \"Example\" is the sample from the original game. Back key: leave a screen / go home.\n\n"
    "Tap anywhere to close.",
    "На доске 56 клеток с цифрами от 0 до 6. Разбейте их на 28 косточек так, чтобы каждая пара значений от 0:0 до 6:6 "
    "встретилась ровно один раз — полный набор.\n\n"
    "Нажмите клетку, затем соседнюю: они станут косточкой. Нажмите готовую косточку, чтобы убрать её. "
    "Справа перечислены все 28 пар; пары, уже стоящие на доске, закрашены чёрным. Если одна пара встретится "
    "дважды, обе её косточки станут чёрными. Побеждает любая полная раскладка со всеми 28 парами, даже если она "
    "отличается от авторской.\n\n"
    "Головоломка «Пример» — образец из оригинальной игры. Клавиша «Назад»: закрыть экран / выйти домой.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Das Brett enthält 56 Felder mit den Ziffern 0 bis 6. Teilen Sie sie in 28 Dominosteine, sodass jedes Wertepaar "
    "von 0:0 bis 6:6 genau einmal vorkommt — der volle Satz.\n\n"
    "Tippen Sie ein Feld an, dann ein benachbartes: beide werden zu einem Stein. Einen fertigen Stein tippen Sie "
    "an, um ihn zu entfernen. Rechts stehen die 28 Paare; Paare, die schon auf dem Brett liegen, sind schwarz. "
    "Kommt ein Paar doppelt vor, werden beide Steine schwarz. Jede vollständige Aufteilung mit allen 28 Paaren "
    "gewinnt, auch wenn sie von der des Buches abweicht.\n\n"
    "Das Rätsel „Beispiel“ stammt aus dem Originalspiel. Zurück-Taste: Bildschirm verlassen / zum Start.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(DOMINOSAVEF, "w");
  int i;
  char buf[DOM_SAVE_LEN + 1];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < DNUM_PUZZLES; i++) {
    dom_save(&boards[i], buf);
    fprintf(f, "%d %s %lld\n", solved[i], buf, stamp[i]);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(DOMINOSAVEF, "r");
  char line[DOM_SAVE_LEN + 48];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < DNUM_PUZZLES) cur = v;
  }
  for (i = 0; i < DNUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *sp = strchr(line, ' ');
    char *sp2;
    if (!sp) continue;
    sp2 = strchr(sp + 1, ' ');
    solved[i] = atoi(line) ? 1 : 0;
    dom_load(&boards[i], sp + 1);
    stamp[i] = sp2 ? strtoll(sp2 + 1, NULL, 10) : 0;
  }
  fclose(f);
}

/* ---------- fonts ---------- */

static void ensure_digit_font(int size) {
  if (f_digit && size == f_digit_size) return;
  if (f_digit) CloseFont(f_digit);
  f_digit = ui_open_font("LiberationSans-Bold", size);
  f_digit_size = size;
}

static void ensure_chip_font(int size) {
  if (f_chip && size == f_chip_size) return;
  if (f_chip) CloseFont(f_chip);
  f_chip = ui_open_font("LiberationSans-Bold", size);
  f_chip_size = size;
}

/* ---------- layout (portrait) ---------- */

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

/* walkthrough: the board and the 28 pairs on top, a counter line, the step text and its buttons below (tutorial.h) */
static void guide_layout(int W, int H) {
  int i, m = 8, gap = 6, pw, area, avail;
  nbtn = 0;
  tut_layout(TUT_DOMINO, gstep, W, H, &gpanel);
  for (i = 0; i < gpanel.nbtn; i++) add_btn(gpanel.btn[i].x, gpanel.btn[i].y, gpanel.btn[i].w, gpanel.btn[i].h, gpanel.btn[i].id, gpanel.btn[i].label);
  chip_gap = 4;
  chip_cols = 2;
  chip_w = W / 12;
  pw = 2 * chip_w + chip_gap;
  gy = 62;
  cs = (W - 2 * m - gap - pw) / 7;
  avail = gpanel.top - 4 - 30 - gy;
  if (avail / 8 < cs) cs = avail / 8;
  area = W - m - pw - gap - m;
  gx = m + (area - 7 * cs) / 2;
  chip_x = W - m - pw;
  chip_y = gy;
  chip_h = (8 * cs - 13 * chip_gap) / 14;
  st_x = m; st_w = W - 2 * m;
  st_y = gy + 8 * cs + 4; st_h = gpanel.top - 2 - st_y;
}

static void layout(int W, int H) {
  int m = 8, gap = 6;
  int tb = 58;
  int by, bh, bx, bw, area;
  if (mode == M_GUIDE) { guide_layout(W, H); return; }
  nbtn = 0;

  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;

  chip_gap = 4;
  gy = tb + 4;
  {
    /* grid on the left, the 28 pairs in two columns on the right (as tall as the grid), status and buttons below */
    int pw;
    chip_cols = 2;
    chip_w = W / 12;
    pw = 2 * chip_w + chip_gap;
    cs = (W - 2 * m - gap - pw) / 7;
    bh = (H > 760) ? 72 : 64;
    by = H - m - bh;
    if ((by - gap - 64 - gy) / 8 < cs) cs = (by - gap - 64 - gy) / 8;
    area = W - m - pw - gap - m;
    gx = m + (area - 7 * cs) / 2;
    chip_x = W - m - pw;
    chip_y = gy;
    chip_h = (8 * cs - 13 * chip_gap) / 14;
    st_x = m; st_w = W - 2 * m;
    st_y = gy + 8 * cs + 6; st_h = by - gap - st_y;
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

static const DomBoard *view(void) { return mode == M_GUIDE ? &gboard : &boards[cur]; }

static void draw_cell(int r, int c, const DomStatus *st) {
  const DomBoard *b = view();
  int i = r * DCOLS + c, j = b->partner[i];
  int x = gx + c * cs, y = gy + r * cs;
  int covered = j >= 0, conflict = covered && st->conflict[i], pending = (b->pending == i);
  int fill = conflict ? BLACK : covered ? 0xDDDDDD : pending ? 0xBBBBBB : WHITE;
  int edge = conflict ? WHITE : BLACK;
  int t = (covered && mode == M_GUIDE && gemph[i]) ? (cs >= 60 ? 5 : 4) : covered ? (cs >= 60 ? 3 : 2) : pending ? 5 : 1;
  char d[2] = {(char)('0' + b->p->grid[r][c]), 0};
  FillArea(x, y, cs, cs, fill);
  /* the sides towards the partner stay open: the two cells read as one tile */
  if (!(covered && j == i - DCOLS)) FillArea(x, y, cs, t, edge);
  if (!(covered && j == i + DCOLS)) FillArea(x, y + cs - t, cs, t, edge);
  if (!(covered && j == i - 1)) FillArea(x, y, t, cs, edge);
  if (!(covered && j == i + 1)) FillArea(x + cs - t, y, t, cs, edge);
  ui_text_center(f_digit, conflict ? WHITE : BLACK, x, y, cs, cs, d);
}

static void draw_board(void) {
  DomStatus st;
  int r, c, k;
  dom_status(view(), &st);
  for (r = 0; r < DROWS; r++)
    for (c = 0; c < DCOLS; c++) draw_cell(r, c, &st);
  /* the grid's outer frame */
  DrawRect(gx - 1, gy - 1, 7 * cs + 2, 8 * cs + 2, BLACK);
  for (k = 0; k < DDOMS; k++) {
    int x = chip_x + (k % chip_cols) * (chip_w + chip_gap), y = chip_y + (k / chip_cols) * (chip_h + chip_gap);
    int used = st.count[CHIP_LO[k]][CHIP_HI[k]] > 0;
    ui_box(x, y, chip_w, chip_h, 1, used ? BLACK : WHITE);
    ui_text_center(f_chip, used ? WHITE : BLACK, x, y, chip_w, chip_h, CHIP_TXT[k]);
  }
}

/* ---------- screens ---------- */

static void draw_status(const DomStatus *st) {
  char t[300];
  const char *extra = "";
  int n;
  if (reset_armed) extra = tr(&S_RESET_AGAIN);
  else if (msg[0]) extra = msg;
  else if (st->nconflicts) extra = tr(&S_DUPLICATE);
  n = snprintf(t, sizeof t, tr(&S_PLACED), st->placed);
  if (extra[0] && n < (int)sizeof t) snprintf(t + n, sizeof t - (size_t)n, "\n%s", extra);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
}

static void draw_play(void) {
  char t[64];
  const DomPuzzle *p = boards[cur].p;
  DomStatus st;
  int i;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), DNUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, DNUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  ui_text_center(ui_f_title, BLACK, title_x, 8, title_w, btns[0].h, t);
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);
  draw_board();
  dom_status(&boards[cur], &st);
  draw_status(&st);
}

static void draw_guide(int W) {
  DomStatus st;
  char t[64];
  tut_draw(TUT_DOMINO, gstep, W, &gpanel);
  draw_board();
  dom_status(&gboard, &st);
  snprintf(t, sizeof t, tr(&S_PLACED), st.placed);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
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

static void redraw(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts(28);
  if (mode == M_GUIDE) dm_tut_board(&DPUZZLES[0], gstep, &gboard, gemph);
  if (mode == M_PLAY || mode == M_GUIDE) {
    layout(W, H);
    ensure_digit_font(cs * 55 / 100 > 18 ? cs * 55 / 100 : 18);
    ensure_chip_font(chip_h * 60 / 100 > 12 ? chip_h * 60 / 100 : 12);
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
  cur = (i % DNUM_PUZZLES + DNUM_PUZZLES) % DNUM_PUZZLES;
  reset_armed = 0; msg[0] = 0;
  boards[cur].pending = -1;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  else if (boards[cur].p->tutorial) strcpy(msg, tr(&TUT_S_HINT));
  save_state();
}

/* returns 1 if the tap completed the puzzle */
static int tap_cell(int r, int c) {
  DomBoard *b = &boards[cur];
  int won = 0;
  reset_armed = 0; msg[0] = 0;
  if (dom_tap(b, r, c)) {
    stamp[cur] = ui_now_ms();
    if (dom_wins(b)) { solved[cur] = 1; won = 1; strcpy(msg, tr(&S_SOLVED_NEXT)); }
    save_state();
  }
  return won;
}

static void do_button(int id) {
  if (id >= 100 && id < 100 + DNUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; }
      else { dom_reset(&boards[cur]); solved[cur] = 0; stamp[cur] = ui_now_ms(); reset_armed = 0; msg[0] = 0; save_state(); }
      break;
    case B_PREV: open_puzzle(cur - 1); break;
    case B_NEXT: open_puzzle(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
    case TUT_B_GUIDE: mode = M_GUIDE; gstep = 0; break;
    case TUT_B_BACK: case TUT_B_NEXT: case TUT_B_SKIP:
      gstep = tut_next_step(TUT_DOMINO, gstep, id);
      if (gstep < 0) { gstep = 0; mode = M_LIST; }
      break;
    case B_HOME: left = 1; shell_leave(); return;
    default: break;
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
  if (x >= gx && x < gx + 7 * cs && y >= gy && y < gy + 8 * cs) {
    redraw(tap_cell((y - gy) / cs, (x - gx) / cs)); /* a fresh solve refreshes the whole panel */
  }
}

/* ---------- Module interface (shell.h) ---------- */

static void domino_init(void) {
  int i, lo, hi, k = 0;
  for (i = 0; i < DNUM_PUZZLES; i++) dom_init(&boards[i], &DPUZZLES[i]);
  for (lo = 0; lo <= 6; lo++)
    for (hi = lo; hi <= 6; hi++) {
      CHIP_LO[k] = lo; CHIP_HI[k] = hi;
      CHIP_TXT[k][0] = (char)('0' + lo); CHIP_TXT[k][1] = ':'; CHIP_TXT[k][2] = (char)('0' + hi); CHIP_TXT[k][3] = 0;
      k++;
    }
  cur = 1;
  load_state();
}

static void domino_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { reset_armed = 0; msg[0] = 0; boards[cur].pending = -1; }
  redraw(1);
}

static int domino_event(int type, int par1, int par2) {
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
        else if (gstep + 1 < TUT_COUNT[TUT_DOMINO]) gstep++;
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

static int domino_prog_count(void) { return DNUM_PUZZLES; }
static const char *domino_prog_id(int i) { return DPUZZLES[i].id; }

static int domino_prog_get(int i, ProgEntry *e) {
  uint8_t l[DDOMS][4];
  int n = dom_list(&boards[i], l), k;
  for (k = 0; k < n; k++) memcpy(e->dom[k], l[k], 4);
  e->ndom = n;
  e->solved = solved[i];
  e->updated_at = stamp[i];
  return n > 0 || solved[i];
}

/* the incoming "placed" list; dominoes that do not fit the board (out of range, not adjacent, overlapping) are skipped */
static void domino_prog_put(int i, const ProgEntry *e) {
  int n;
  dom_reset(&boards[i]);
  for (n = 0; n < e->ndom; n++) dom_place(&boards[i], e->dom[n][0], e->dom[n][1], e->dom[n][2], e->dom[n][3]);
  solved[i] = e->solved ? 1 : 0;
  stamp[i] = e->updated_at;
  if (i == cur) { reset_armed = 0; msg[0] = 0; }
}

/* the sample puzzle (index 0) is a tutorial and not counted */
static int domino_count(void) { return DNUM_PUZZLES - 1; }

static int domino_solved(void) {
  int i, n = 0;
  for (i = 1; i < DNUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int domino_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < DNUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (DNUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < DNUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_domino = {
    "domino",
    S("Domino Solitaire", "Домино-пасьянс", "Domino-Patience"),
    S("Rebuild the 28 dominoes hidden in a grid of digits - the full 0:0 to 6:6 set.",
      "Восстановите 28 косточек домино в сетке из цифр — полный набор от 0:0 до 6:6.",
      "Setzen Sie die 28 Dominosteine im Zahlengitter wieder zusammen — der volle Satz von 0:0 bis 6:6."),
    domino_init,
    domino_enter,
    domino_event,
    save_state,
    domino_count,
    domino_solved,
    domino_pick_random,
    "module_domino",
    domino_prog_count,
    domino_prog_id,
    domino_prog_get,
    domino_prog_put,
};
