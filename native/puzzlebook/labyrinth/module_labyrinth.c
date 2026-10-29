/* Alphabet Labyrinth: a Puzzle Book module for the PocketBook 625.
 * Game rules and data: labyrinth_core.c / labyrinth_data.h (port of il721/puzzles, module_labyrinth).
 * UI: tap a cell next to either end of the path to extend it (the two ends grow towards each other), tap an end to
 * take a cell back, tap any other cell to mark it "must be on the path" (a black frame). The Cross button switches
 * to crossing out: tap a cell to cross it out, tap a path cell to cut the path back to it (the source's long press;
 * the reader has no long-press event we rely on). Letters that share a letter with the path are dimmed
 * automatically. As in the source the Cyrillic letters are shown only in Russian; English and German show the
 * source's stand-in glyphs. Progress in /mnt/ext1/labyrinth.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "labyrinth_core.h"
#include "labyrinth_data.h"

#ifndef LABYRINTHSAVEF
#define LABYRINTHSAVEF "/mnt/ext1/labyrinth.sav"
#endif

enum { B_CROSS = 10, B_RESET, B_PREV, B_NEXT, B_LIST, B_HELP, B_HOME, B_CLOSE };
enum { M_PLAY, M_LIST, M_HELP };

typedef UiBtn Btn;

static LabBoard boards[LNUM_PUZZLES];
static int solved[LNUM_PUZZLES];
static long long stamp[LNUM_PUZZLES]; /* ms since 1970 of the last change to each puzzle, 0 = unknown */
static int cur = 1, mode = M_PLAY;
static int tool_cross, reset_armed, updates;
static char msg[200];
static int left; /* set when the module handed control back to the shell */

static Btn btns[32];
static int nbtn;
static int title_x, title_w;
static int gx, gy, cs;                                         /* grid origin and cell size */
static int chip_x, chip_y, chip_w, chip_h, chip_cols, chip_gap; /* the alphabet tracker */
static int st_x, st_y, st_w, st_h;                              /* status area */

static ifont *f_letter, *f_chip;
static int f_letter_size, f_chip_size;

/* the letters as shown: Russian shows the Cyrillic alphabet, English and German the source's stand-in glyphs */
static const char *const RU_LETTER[33] = {"А", "Б", "В", "Г", "Д", "Е", "Ё", "Ж", "З", "И", "Й", "К", "Л", "М", "Н", "О", "П",
                                          "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ч", "Ш", "Щ", "Ъ", "Ы", "Ь", "Э", "Ю", "Я"};
static const char *const GLYPH[33] = {"A", "B", "V", "G", "D", "E", "=", "*", "3", "I", "J", "K", "L", "M", "N", "O", "P",
                                      "R", "S", "T", "U", "F", "X", "C", "Q", "W", "#", "&", "$", "@", "H", "Y", "Z"};

static const char *letter_text(int i) { return lang() == LANG_RU ? RU_LETTER[i] : GLYPH[i]; }

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_CROSS = S("Cross", "Крест", "Kreuz");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_EXAMPLE = S("Example  (1/%d)", "Пример  (1/%d)", "Beispiel  (1/%d)");
static const Str S_PUZZLE = S("Labyrinth %d/%d%s", "Лабиринт %d/%d%s", "Labyrinth %d/%d%s");
static const Str S_EX_SHORT = S("Ex", "Пр", "Bsp");
static const Str S_LIST_TITLE = S("Alphabet Labyrinth - choose a puzzle", "Выберите лабиринт", "Labyrinth wählen");
static const Str S_HELP_TITLE = S("How to play", "Как играть", "Spielregeln");
static const Str S_LETTERS = S("Letters: %d of 33", "Букв: %d из 33", "Buchstaben: %d von 33");
static const Str S_DUPLICATE = S("A letter is used twice (shown in black).", "Буква использована дважды (чёрная).",
                                 "Ein Buchstabe doppelt (schwarz markiert).");
static const Str S_CROSS_HINT = S("Cross mode: tap to cross out or cut the path.",
                                  "Режим «Крест»: вычеркнуть или обрезать путь.",
                                  "Streichmodus: streichen oder Pfad kürzen.");
static const Str S_SOLVED_BEFORE = S("Solved before. Edit freely or press Reset.", "Уже решено. Можно менять или «Сбросить».",
                                     "Schon gelöst. Ändern oder „Reset“.");
static const Str S_SOLVED_NEXT = S("Solved! Press > for the next puzzle.", "Решено! Нажмите > для следующей.",
                                   "Gelöst! Mit > zum nächsten Rätsel.");
static const Str S_RESET_AGAIN = S("Tap Reset again to clear the path.", "«Сбросить» ещё раз — стереть путь.",
                                   "Noch einmal „Reset“: Pfad löschen.");

static const Str S_HELP_TEXT = S(
    "An 8x8 grid of letters. Thread a single path from the top-left corner to the bottom-right one so that it passes "
    "through every letter of the alphabet exactly once (33 cells). In English and German the letters of the Russian "
    "alphabet are shown as stand-in symbols, as in the original game.\n\n"
    "Tap a cell next to either end of the path to extend it; tap an end to take a cell back. The two ends grow towards "
    "each other and the path is complete when they meet. Tapping a cell that is not next to the path puts a frame "
    "on it: that letter must be on the path.\n\n"
    "Cross switches to crossing out: tap a cell to cross it out (it will not be on the path), or tap a path cell to cut "
    "the path back to it. As soon as one of several identical letters joins the path, the others are dimmed "
    "automatically. The alphabet below the grid shows the letters used so far; a letter used twice turns black.\n\n"
    "Puzzle \"Example\" is the sample from the original game. Back key: leave a screen / go home.\n\n"
    "Tap anywhere to close.",
    "Сетка 8×8 из букв. Проведите единственный путь из левого верхнего угла в правый нижний так, чтобы он прошёл через "
    "каждую букву алфавита ровно один раз (33 клетки).\n\n"
    "Нажмите клетку рядом с концом пути, чтобы продлить его; нажмите конец, чтобы убрать клетку. Оба конца растут "
    "навстречу друг другу, и путь готов, когда они встречаются. Нажатие на клетку, не соседнюю с путём, обводит её "
    "рамкой: эта буква обязана быть на пути.\n\n"
    "Кнопка «Крест» включает вычёркивание: нажмите клетку, чтобы вычеркнуть её (её не будет на пути), или нажмите "
    "клетку пути, чтобы обрезать путь до неё. Как только одна из нескольких одинаковых букв попадает на путь, "
    "остальные автоматически тускнеют. Алфавит под сеткой показывает использованные буквы; буква, взятая дважды, "
    "становится чёрной.\n\n"
    "Головоломка «Пример» — образец из оригинальной игры. Клавиша «Назад»: закрыть экран / выйти домой.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Ein 8x8-Raster aus Buchstaben. Ziehen Sie einen einzigen Pfad von der oberen linken zur unteren rechten Ecke, der "
    "jeden Buchstaben des Alphabets genau einmal berührt (33 Felder). In Englisch und Deutsch werden die Buchstaben "
    "des russischen Alphabets wie im Originalspiel durch Ersatzzeichen dargestellt.\n\n"
    "Tippen Sie ein Feld neben einem Pfadende an, um den Pfad zu verlängern; ein Ende antippen nimmt ein Feld zurück. "
    "Beide Enden wachsen aufeinander zu, und der Pfad ist fertig, wenn sie sich treffen. Ein Tipp auf ein Feld, das "
    "nicht am Pfad liegt, setzt einen Rahmen darum: dieser Buchstabe muss auf dem Pfad liegen.\n\n"
    "„Kreuz“ schaltet auf Durchstreichen um: Feld antippen zum Durchstreichen (es liegt nicht auf dem Pfad) oder "
    "ein Pfadfeld antippen, um den Pfad bis dorthin zu kürzen. Sobald einer von mehreren gleichen Buchstaben auf dem "
    "Pfad liegt, werden die übrigen automatisch abgeblendet. Das Alphabet unter dem Raster zeigt die bisher "
    "verwendeten Buchstaben; ein doppelt verwendeter wird schwarz.\n\n"
    "Das Rätsel „Beispiel“ stammt aus dem Originalspiel. Zurück-Taste: Bildschirm verlassen / zum Start.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* ---------- persistence ---------- */

static void save_state(void) {
  FILE *f = fopen(LABYRINTHSAVEF, "w");
  int i;
  char buf[LAB_SAVE_LEN];
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (i = 0; i < LNUM_PUZZLES; i++) {
    lab_save(&boards[i], buf);
    fprintf(f, "%d %s %lld\n", solved[i], buf, stamp[i]);
  }
  fclose(f);
}

/* the next space-separated field of *p (NUL-terminated in place), or NULL at the end of the line */
static char *next_field(char **p) {
  char *s = *p, *e;
  if (!s || !*s) return NULL;
  e = strchr(s, ' ');
  if (e) { *e = 0; *p = e + 1; } else *p = NULL;
  return s;
}

static void load_state(void) {
  FILE *f = fopen(LABYRINTHSAVEF, "r");
  char line[LAB_SAVE_LEN + 64];
  int i, v;
  if (!f) return;
  /* atoi instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line, sizeof line, f) && strncmp(line, "v1 ", 3) == 0) {
    v = atoi(line + 3);
    if (v >= 0 && v < LNUM_PUZZLES) cur = v;
  }
  for (i = 0; i < LNUM_PUZZLES && fgets(line, sizeof line, f); i++) {
    char *p = line, *fs, *fa, *fz, *fx, *fm, *ft;
    line[strcspn(line, "\n")] = 0;
    fs = next_field(&p); fa = next_field(&p); fz = next_field(&p); fx = next_field(&p); fm = next_field(&p); ft = next_field(&p);
    if (!fs || !fa || !fz || !fx || !fm) continue;
    solved[i] = atoi(fs) ? 1 : 0;
    lab_load(&boards[i], fa, fz, fx, fm);
    stamp[i] = ft ? strtoll(ft, NULL, 10) : 0;
  }
  fclose(f);
}

/* ---------- fonts ---------- */

static void ensure_letter_font(int size) {
  if (f_letter && size == f_letter_size) return;
  if (f_letter) CloseFont(f_letter);
  f_letter = ui_open_font("LiberationSans-Bold", size);
  f_letter_size = size;
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

static void layout(int W, int H) {
  int m = 8, gap = 6;
  int tb = 54;
  nbtn = 0;

  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;

  chip_gap = 3;
  gy = tb + 2;
  {
    /* grid on top; below it the alphabet (3 rows of 11), the status and one row of four buttons */
    int bh = 60, by, trk_h, w4 = (W - 2 * m - 3 * gap) / 4;
    chip_cols = 11;
    chip_w = (W - 2 * m - (chip_cols - 1) * chip_gap) / chip_cols;
    chip_h = 32;
    trk_h = 3 * chip_h + 2 * chip_gap;
    st_h = 52;
    by = H - m - bh;
    cs = (by - gap - st_h - gap - trk_h - gap - gy) / 8;
    if ((W - 2 * m) / 8 < cs) cs = (W - 2 * m) / 8;
    gx = m + (W - 2 * m - 8 * cs) / 2;
    chip_x = m; chip_y = gy + 8 * cs + gap;
    st_x = m; st_w = W - 2 * m; st_y = chip_y + trk_h + gap; st_h = by - gap - st_y;
    add_btn(m, by, w4, bh, B_CROSS, tr(&S_CROSS));
    add_btn(m + (w4 + gap), by, w4, bh, B_PREV, "<");
    add_btn(m + 2 * (w4 + gap), by, w4, bh, B_RESET, tr(&S_RESET));
    add_btn(m + 3 * (w4 + gap), by, w4, bh, B_NEXT, ">");
  }
}

/* ---------- board drawing ---------- */

static int cx_of(int cell) { return gx + (cell % 8) * cs + cs / 2; }
static int cy_of(int cell) { return gy + (cell / 8) * cs + cs / 2; }

/* the bar of a path step between two adjacent cells */
static void draw_step(int c1, int c2, int bt) {
  int x1 = cx_of(c1), y1 = cy_of(c1), x2 = cx_of(c2), y2 = cy_of(c2);
  int x = (x1 < x2 ? x1 : x2) - bt / 2, y = (y1 < y2 ? y1 : y2) - bt / 2;
  int w = (x1 < x2 ? x2 - x1 : x1 - x2) + bt, h = (y1 < y2 ? y2 - y1 : y1 - y2) + bt;
  FillArea(x, y, w, h, 0x999999);
}

/* a diagonal cross over a cell, as a staircase of small squares (no line drawing call needed) */
static void draw_x(int x, int y, int s) {
  int i, in = s / 6, t = s / 14 > 2 ? s / 14 : 2, n = s - 2 * in - t;
  for (i = 0; i <= n; i++) {
    FillArea(x + in + i, y + in + i, t, t, BLACK);
    FillArea(x + in + i, y + s - in - t - i, t, t, BLACK);
  }
}

static void draw_ring(int cell, int color) {
  int x = gx + (cell % 8) * cs, y = gy + (cell / 8) * cs, k, t = cs >= 60 ? 4 : 3;
  for (k = 0; k < t; k++) DrawRect(x + 3 + k, y + 3 + k, cs - 6 - 2 * k, cs - 6 - 2 * k, color);
}

static void draw_board(void) {
  const LabBoard *b = &boards[cur];
  LabStatus st;
  int r, c, i, bt = cs * 16 / 100 > 4 ? cs * 16 / 100 : 4;
  lab_status(b, &st);

  for (r = 0; r < 8; r++)
    for (c = 0; c < 8; c++) {
      int cell = r * 8 + c, onp = lab_on_path(b, cell);
      int dup = onp && st.used[b->p->letter[cell]] > 1;
      int x = gx + c * cs, y = gy + r * cs;
      FillArea(x, y, cs, cs, dup ? BLACK : onp ? 0xDDDDDD : WHITE);
      DrawRect(x, y, cs, cs, BLACK);
    }
  for (i = 1; i < b->na; i++) draw_step(b->a[i - 1], b->a[i], bt);
  for (i = 1; i < b->nz; i++) draw_step(b->z[i - 1], b->z[i], bt);
  draw_ring(b->a[b->na - 1], st.used[b->p->letter[b->a[b->na - 1]]] > 1 ? WHITE : BLACK);
  draw_ring(b->z[b->nz - 1], st.used[b->p->letter[b->z[b->nz - 1]]] > 1 ? WHITE : BLACK);
  for (r = 0; r < 8; r++)
    for (c = 0; c < 8; c++) {
      int cell = r * 8 + c, x = gx + c * cs, y = gy + r * cs, k;
      if (b->mark[cell]) for (k = 0; k < 4; k++) DrawRect(x + 2 + k, y + 2 + k, cs - 4 - 2 * k, cs - 4 - 2 * k, BLACK);
      if (b->cross[cell]) draw_x(x, y, cs);
    }
  /* the letters, always all 64 in reading order */
  for (r = 0; r < 8; r++)
    for (c = 0; c < 8; c++) {
      int cell = r * 8 + c, onp = lab_on_path(b, cell);
      int dup = onp && st.used[b->p->letter[cell]] > 1;
      int color = dup ? WHITE : b->cross[cell] ? 0x777777 : st.auto_cross[cell] ? 0xAAAAAA : BLACK;
      ui_text_center(f_letter, color, gx + c * cs, gy + r * cs, cs, cs, letter_text(b->p->letter[cell]));
    }
  /* the alphabet: white = still to place, grey with a line through = on the path, black = used twice */
  for (i = 0; i < 33; i++) {
    int x = chip_x + (i % chip_cols) * (chip_w + chip_gap), y = chip_y + (i / chip_cols) * (chip_h + chip_gap);
    int u = st.used[i];
    ui_box(x, y, chip_w, chip_h, 1, u > 1 ? BLACK : u == 1 ? 0xDDDDDD : WHITE);
    ui_text_center(f_chip, u > 1 ? WHITE : u == 1 ? 0x777777 : BLACK, x, y, chip_w, chip_h, letter_text(i));
    if (u == 1) FillArea(x + 4, y + chip_h / 2, chip_w - 8, 2, 0x777777);
  }
}

/* ---------- screens ---------- */

static void draw_status(const LabStatus *st) {
  char t[400];
  const char *extra = "";
  int n;
  if (reset_armed) extra = tr(&S_RESET_AGAIN);
  else if (msg[0]) extra = msg;
  else if (st->ndup) extra = tr(&S_DUPLICATE);
  else if (tool_cross) extra = tr(&S_CROSS_HINT);
  n = snprintf(t, sizeof t, tr(&S_LETTERS), st->placed);
  if (extra[0] && n < (int)sizeof t) snprintf(t + n, sizeof t - (size_t)n, "\n%s", extra);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
}

/* the Cross button shows its state: black while crossing out is on */
static void draw_cross_btn(const Btn *b) {
  if (!tool_cross) { ui_draw_btn(b); return; }
  ui_box(b->x, b->y, b->w, b->h, 2, BLACK);
  ui_text_center(StringWidth(b->label) > b->w - 16 ? ui_f_small : ui_f_btn, WHITE, b->x, b->y, b->w, b->h, b->label);
}

static void draw_play(void) {
  char t[64];
  const LabPuzzle *p = boards[cur].p;
  LabStatus st;
  int i;
  if (p->tutorial) snprintf(t, sizeof t, tr(&S_EXAMPLE), LNUM_PUZZLES);
  else snprintf(t, sizeof t, tr(&S_PUZZLE), cur, LNUM_PUZZLES - 1, solved[cur] ? "  ok" : "");
  ui_text_center(ui_f_title, BLACK, title_x, 8, title_w, btns[0].h, t);
  for (i = 0; i < nbtn; i++) {
    if (btns[i].id == B_CROSS) { SetFont(ui_f_btn, BLACK); draw_cross_btn(&btns[i]); }
    else ui_draw_btn(&btns[i]);
  }
  draw_board();
  lab_status(&boards[cur], &st);
  draw_status(&st);
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 10, cols = 4, rows = (LNUM_PUZZLES + cols - 1) / cols;
  int bw = (W - 2 * m - (cols - 1) * gap) / cols, bh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (bh > bw) bh = bw;
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < LNUM_PUZZLES; i++) {
    int x = m + (i % cols) * (bw + gap), y = top + (i / cols) * (bh + gap);
    char t[24];
    if (boards[i].p->tutorial) snprintf(t, sizeof t, "%s%s", tr(&S_EX_SHORT), solved[i] ? " ok" : "");
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
  ui_fonts(28);
  if (mode == M_PLAY) {
    layout(W, H);
    ensure_letter_font(cs * 50 / 100 > 18 ? cs * 50 / 100 : 18);
    ensure_chip_font(chip_h * 58 / 100 > 12 ? chip_h * 58 / 100 : 12);
  }
  ClearScreen();
  if (mode == M_PLAY) draw_play();
  else if (mode == M_LIST) draw_list(W, H);
  else draw_help(W, H);
  updates++;
  if (full || updates % 8 == 0) FullUpdate();
  else PartialUpdate(0, 0, W, H);
}

/* ---------- actions ---------- */

static void open_puzzle(int i) {
  cur = (i % LNUM_PUZZLES + LNUM_PUZZLES) % LNUM_PUZZLES;
  reset_armed = 0; msg[0] = 0;
  if (solved[cur]) strcpy(msg, tr(&S_SOLVED_BEFORE));
  save_state();
}

/* returns 1 if the tap completed the puzzle */
static int tap_cell(int cell) {
  LabBoard *b = &boards[cur];
  int won = 0;
  reset_armed = 0; msg[0] = 0;
  if (tool_cross ? lab_cross(b, cell) : lab_tap(b, cell)) {
    stamp[cur] = ui_now_ms();
    if (lab_wins(b)) { solved[cur] = 1; won = 1; strcpy(msg, tr(&S_SOLVED_NEXT)); }
    save_state();
  }
  return won;
}

static void do_button(int id) {
  if (id >= 100 && id < 100 + LNUM_PUZZLES) { mode = M_PLAY; open_puzzle(id - 100); return; }
  switch (id) {
    case B_CROSS: tool_cross = !tool_cross; reset_armed = 0; break;
    case B_RESET:
      if (!reset_armed) { reset_armed = 1; }
      else { lab_reset(&boards[cur]); solved[cur] = 0; stamp[cur] = ui_now_ms(); reset_armed = 0; msg[0] = 0; save_state(); }
      break;
    case B_PREV: open_puzzle(cur - 1); break;
    case B_NEXT: open_puzzle(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
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
  if (x >= gx && x < gx + 8 * cs && y >= gy && y < gy + 8 * cs)
    redraw(tap_cell(((y - gy) / cs) * 8 + (x - gx) / cs)); /* a fresh solve refreshes the whole panel */
}

/* ---------- Module interface (shell.h) ---------- */

static void labyrinth_init(void) {
  int i;
  for (i = 0; i < LNUM_PUZZLES; i++) lab_init(&boards[i], &LPUZZLES[i]);
  cur = 1;
  load_state();
}

static void labyrinth_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_puzzle(puzzle);
  else { reset_armed = 0; msg[0] = 0; }
  redraw(1);
}

static int labyrinth_event(int type, int par1, int par2) {
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

/* ---------- progress file hooks (shell.h) ---------- */

static int labyrinth_prog_count(void) { return LNUM_PUZZLES; }
static const char *labyrinth_prog_id(int i) { return LPUZZLES[i].id; }

static void put_list(ProgEntry *e, int l, const int8_t *cells, int n) {
  int k;
  for (k = 0; k < n; k++) { e->list[l][k][0] = (unsigned char)(cells[k] / 8); e->list[l][k][1] = (unsigned char)(cells[k] % 8); }
  e->nlist[l] = n;
}

static int labyrinth_prog_get(int i, ProgEntry *e) {
  const LabBoard *b = &boards[i];
  int8_t t[LCELLS];
  int c, n;
  put_list(e, 0, b->a, b->na);
  put_list(e, 1, b->z, b->nz);
  for (c = 0, n = 0; c < LCELLS; c++) if (b->cross[c]) t[n++] = (int8_t)c;
  put_list(e, 2, t, n);
  for (c = 0, n = 0; c < LCELLS; c++) if (b->mark[c]) t[n++] = (int8_t)c;
  put_list(e, 3, t, n);
  e->has_lists = 1;
  e->solved = solved[i];
  e->updated_at = stamp[i];
  return solved[i] || b->na > 1 || b->nz > 1 || e->nlist[2] || e->nlist[3];
}

/* the source's fromJson: lists that do not make a valid board give a fresh one (the solved flag is kept) */
static void labyrinth_prog_put(int i, const ProgEntry *e) {
  uint8_t cells[4][PROG_MAX_LIST];
  int l, k;
  for (l = 0; l < 4; l++)
    for (k = 0; k < e->nlist[l]; k++) {
      int r = e->list[l][k][0], c = e->list[l][k][1];
      cells[l][k] = (r < 8 && c < 8) ? (uint8_t)(r * 8 + c) : 255;
    }
  lab_restore(&boards[i], cells[0], e->nlist[0], cells[1], e->nlist[1], cells[2], e->nlist[2], cells[3], e->nlist[3]);
  solved[i] = e->solved ? 1 : 0;
  stamp[i] = e->updated_at;
  if (i == cur) { reset_armed = 0; msg[0] = 0; }
}

/* the sample puzzle (index 0) is a tutorial and not counted */
static int labyrinth_count(void) { return LNUM_PUZZLES - 1; }

static int labyrinth_solved(void) {
  int i, n = 0;
  for (i = 1; i < LNUM_PUZZLES; i++) n += solved[i];
  return n;
}

static int labyrinth_pick_random(void) {
  int i, n = 0, want;
  for (i = 1; i < LNUM_PUZZLES; i++) n += !solved[i];
  if (n == 0) return 1 + rand() % (LNUM_PUZZLES - 1);
  want = rand() % n;
  for (i = 1; i < LNUM_PUZZLES; i++)
    if (!solved[i] && want-- == 0) return i;
  return 1;
}

const Module module_labyrinth = {
    "labyrinth",
    S("Alphabet Labyrinth", "Лабиринт-алфавит", "Buchstaben-Labyrinth"),
    S("Thread a single path across the 8x8 grid from corner to corner, using each letter of the alphabet exactly once.",
      "Проведите единственный путь по сетке 8×8 от А до Я, используя каждую букву алфавита ровно один раз.",
      "Ziehen Sie einen einzigen Pfad von Ecke zu Ecke durchs 8x8-Raster und verwenden Sie jeden Buchstaben des Alphabets genau einmal."),
    labyrinth_init,
    labyrinth_enter,
    labyrinth_event,
    save_state,
    labyrinth_count,
    labyrinth_solved,
    labyrinth_pick_random,
    "module_labyrinth",
    labyrinth_prog_count,
    labyrinth_prog_id,
    labyrinth_prog_get,
    labyrinth_prog_put,
};
