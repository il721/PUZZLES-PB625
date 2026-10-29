/* Puzzle Book shell: home screen, module routing, random-puzzle button. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shell.h"

enum { H_RANDOM = 1000, H_HELP, H_EXIT, H_PREV, H_NEXT, H_TILE = 2000 };

static const Module *active; /* NULL = home screen */
static int home_help, page, updates;

static UiBtn btns[40];
static int nbtn;

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  UiBtn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

static const char *HELP_TEXT =
    "Puzzle Book bundles several puzzle games. Tap a game to open it; it remembers where you were.\n\n"
    "Random opens a random puzzle you have not solved yet, from any game.\n\n"
    "Inside a game, Home (or the Back key) returns here and every game has its own rules (?).\n\n"
    "Progress is stored on the reader's drive as <game>.sav next to the Applications folder.\n\n"
    "Tap anywhere to close.";

/* One tile per row; if the modules do not fit on one screen a paging bar takes the bottom. */
static int per_page(int W, int H, int *cols, int *tile_h) {
  int rows, tb = 58, area_h = H - tb - 16;
  *cols = 1;
  *tile_h = (W > H) ? 130 : 150;
  rows = area_h / (*tile_h + 10);
  if (rows < NUM_MODULES) rows = (area_h - 70) / (*tile_h + 10);
  if (rows < 1) rows = 1;
  return *cols * rows;
}

static void draw_home(int W, int H) {
  int m = 8, gap = 10, tb = 58, cols, tile_h, pp = per_page(W, H, &cols, &tile_h), i, first, pages;
  int tile_w = (W - 2 * m - (cols - 1) * gap) / cols;
  char t[48];
  pages = (NUM_MODULES + pp - 1) / pp;
  if (page >= pages) page = pages - 1;
  if (page < 0) page = 0;
  first = page * pp;

  nbtn = 0;
  add_btn(W - m - 90, m, 90, tb - m, H_EXIT, "Exit");
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, H_HELP, "?");
  add_btn(W - m - 90 - 2 * gap - 60 - 150, m, 150, tb - m, H_RANDOM, "Random");
  ui_text_center(ui_f_title, BLACK, m, m, W - 2 * m - 90 - 2 * gap - 60 - 150 - gap, tb - m, "Puzzle Book");
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);

  for (i = first; i < NUM_MODULES && i < first + pp; i++) {
    const Module *mod = MODULES[i];
    int idx = i - first, x = m + (idx % cols) * (tile_w + gap), y = tb + 8 + (idx / cols) * (tile_h + 10);
    ui_box(x, y, tile_w, tile_h, 3, WHITE);
    SetFont(ui_f_title, BLACK);
    DrawString(x + 16, y + 12, mod->title);
    snprintf(t, sizeof t, "%d / %d solved", mod->solved_count(), mod->puzzle_count());
    SetFont(ui_f_small, BLACK);
    DrawString(x + tile_w - 16 - StringWidth(t), y + 14, t);
    DrawTextRect(x + 16, y + 20 + ui_f_title->height, tile_w - 32, tile_h - 28 - ui_f_title->height, (char *)mod->description, ALIGN_LEFT);
    add_btn(x, y, tile_w, tile_h, H_TILE + i, "");
  }
  if (pages > 1) {
    int by = H - m - 62;
    add_btn(m, by, 120, 62, H_PREV, "<");
    add_btn(W - m - 120, by, 120, 62, H_NEXT, ">");
    ui_draw_btn(&btns[nbtn - 2]);
    ui_draw_btn(&btns[nbtn - 1]);
    snprintf(t, sizeof t, "%d / %d", page + 1, pages);
    ui_text_center(ui_f_btn, BLACK, 0, by, W, 62, t);
  }
}

static void draw_help(int W, int H) {
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, "Puzzle Book - help");
  SetFont(ui_f_small, BLACK);
  DrawTextRect(16, 64, W - 32, H - 80, (char *)HELP_TEXT, ALIGN_LEFT);
}

static void redraw_home(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts((W > H) ? 26 : 28);
  ClearScreen();
  if (home_help) draw_help(W, H);
  else draw_home(W, H);
  updates++;
  if (full || updates % 8 == 0) FullUpdate();
  else PartialUpdate(0, 0, W, H);
}

static void shell_redraw(void) {
  if (active) active->event(EVT_SHOW, 0, 0);
  else redraw_home(1);
}

void shell_leave(void) {
  if (active) active->save();
  active = NULL;
  home_help = 0;
  redraw_home(1);
}

static void open_module(const Module *m, int puzzle) {
  active = m;
  m->enter(puzzle);
}

static void random_puzzle(void) {
  int i, total = 0, pick;
  for (i = 0; i < NUM_MODULES; i++) total += MODULES[i]->puzzle_count() - MODULES[i]->solved_count();
  if (total > 0) { /* weight modules by how many unsolved puzzles they still have */
    pick = rand() % total;
    for (i = 0; i < NUM_MODULES; i++) {
      int left = MODULES[i]->puzzle_count() - MODULES[i]->solved_count();
      if (pick < left) break;
      pick -= left;
    }
  } else {
    i = rand() % NUM_MODULES;
  }
  if (i >= NUM_MODULES) i = 0;
  open_module(MODULES[i], MODULES[i]->pick_random());
}

static void home_tap(int x, int y) {
  int i;
  if (home_help) { home_help = 0; redraw_home(1); return; }
  for (i = 0; i < nbtn; i++) {
    const UiBtn *b = &btns[i];
    if (!ui_hit(b, x, y)) continue;
    if (b->id >= H_TILE) open_module(MODULES[b->id - H_TILE], -1);
    else if (b->id == H_RANDOM) random_puzzle();
    else if (b->id == H_HELP) { home_help = 1; redraw_home(1); }
    else if (b->id == H_EXIT) CloseApp();
    else if (b->id == H_PREV) { page--; redraw_home(1); }
    else if (b->id == H_NEXT) { page++; redraw_home(1); }
    return;
  }
}

static int shell_handler(int type, int par1, int par2) {
  int i;
  switch (type) {
    case EVT_INIT:
      srand((unsigned)time(NULL));
      for (i = 0; i < NUM_MODULES; i++) MODULES[i]->init();
      ui_log("init %dx%d, modules %d", ScreenWidth(), ScreenHeight(), NUM_MODULES);
      break;
    case EVT_SHOW:
      shell_redraw();
      break;
    case EVT_ORIENTATION:
      SetOrientation(par1);
      ui_log("orientation %d -> %dx%d", par1, ScreenWidth(), ScreenHeight());
      shell_redraw();
      break;
    case EVT_EXIT:
    case EVT_SAVESTATE:
      for (i = 0; i < NUM_MODULES; i++) MODULES[i]->save();
      break;
    case EVT_POINTERUP:
      if (active) active->event(type, par1, par2);
      else home_tap(par1, par2);
      break;
    case EVT_KEYPRESS:
      if (active) active->event(type, par1, par2);
      else if (par1 == KEY_BACK) {
        if (home_help) { home_help = 0; redraw_home(1); }
        else CloseApp();
      }
      break;
    default:
      break;
  }
  return 0;
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  InkViewMain(shell_handler);
  return 0;
}
