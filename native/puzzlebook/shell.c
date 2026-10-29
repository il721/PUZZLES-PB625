/* Puzzle Book shell: home screen, module routing, random-puzzle button. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shell.h"

enum { H_RANDOM = 1000, H_HELP, H_EXIT, H_PREV, H_NEXT, H_SETTINGS, H_BACK, H_LANG = 1100, H_TILE = 2000 };
enum { HM_HOME, HM_HELP, HM_SETTINGS };

static const Module *active; /* NULL = home screen */
static int home_mode = HM_HOME, page, updates;

static UiBtn btns[40];
static int nbtn;

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  UiBtn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

static const Str S_TITLE = S("Puzzle Book", "Головоломки", "Rätselbuch");
static const Str S_EXIT = S("Exit", "Выход", "Beenden");
static const Str S_RANDOM = S("Random", "Случайный", "Zufall");
static const Str S_SETTINGS = S("Settings", "Настройки", "Einstellungen");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_SOLVED = S("%d / %d solved", "решено %d / %d", "gelöst %d / %d");
static const Str S_HELP_TITLE = S("Puzzle Book - help", "Головоломки - справка", "Rätselbuch - Hilfe");
static const Str S_LANGUAGE = S("Language", "Язык", "Sprache");
static const Str S_LANG_SYSTEM = S("System default", "Как в системе", "Systemsprache");
static const Str S_LANG_RU = S("Русский", "Русский", "Русский");
static const Str S_LANG_EN = S("English", "English", "English");
static const Str S_LANG_DE = S("Deutsch", "Deutsch", "Deutsch");

static const Str S_HELP_TEXT = S(
    "Puzzle Book bundles several puzzle games. Tap a game to open it; it remembers where you were.\n\n"
    "Random opens a random puzzle you have not solved yet, from any game.\n\n"
    "Settings: the language of the app (like the reader's own, or fixed).\n\n"
    "Inside a game, Home (or the Back key) returns here and every game has its own rules (?).\n\n"
    "Progress is stored on the reader's drive as <game>.sav next to the Applications folder.\n\n"
    "Tap anywhere to close.",
    "В «Головоломках» собрано несколько игр. Нажмите на игру, чтобы открыть её; она помнит, где вы остановились.\n\n"
    "«Случайный» открывает случайную нерешённую головоломку из любой игры.\n\n"
    "«Настройки»: язык приложения (как в самом ридере или любой другой).\n\n"
    "Внутри игры кнопка «Домой» (или клавиша «Назад») возвращает сюда; у каждой игры свои правила (?).\n\n"
    "Прогресс хранится на диске ридера в файлах <игра>.sav рядом с папкой Applications.\n\n"
    "Нажмите в любом месте, чтобы закрыть.",
    "Das Rätselbuch bündelt mehrere Rätselspiele. Tippen Sie ein Spiel an, um es zu öffnen; es merkt sich, wo Sie waren.\n\n"
    "„Zufall“ öffnet ein zufälliges, noch ungelöstes Rätsel aus einem beliebigen Spiel.\n\n"
    "„Einstellungen“: die Sprache der App (wie beim Reader selbst oder fest gewählt).\n\n"
    "In einem Spiel führt „Start“ (oder die Zurück-Taste) hierher zurück; jedes Spiel hat eigene Regeln (?).\n\n"
    "Der Fortschritt liegt auf dem Laufwerk des Readers als <Spiel>.sav neben dem Ordner Applications.\n\n"
    "Tippen Sie irgendwo, um zu schließen.");

/* One tile per row; the bottom bar (Settings, paging) is always there. */
static int per_page(int W, int H, int *cols, int *tile_h) {
  int rows, tb = 58, area_h = H - tb - 16 - 70;
  (void)W;
  *cols = 1;
  *tile_h = (W > H) ? 130 : 150;
  rows = area_h / (*tile_h + 10);
  if (rows < 1) rows = 1;
  return *cols * rows;
}

/* button width for a label: wide enough, within [min, max]; ui_draw_btn shrinks the font if it still does not fit */
static int label_w(const char *s, int min, int max) {
  int w;
  SetFont(ui_f_btn, BLACK);
  w = StringWidth(s) + 40;
  return w < min ? min : (w > max ? max : w);
}

static void draw_home(int W, int H) {
  int m = 8, gap = 10, tb = 58, cols, tile_h, pp = per_page(W, H, &cols, &tile_h), i, first, pages;
  int tile_w = (W - 2 * m - (cols - 1) * gap) / cols, x, by = H - m - 62;
  const char *exit_t = tr(&S_EXIT), *rand_t = tr(&S_RANDOM), *set_t = tr(&S_SETTINGS);
  char t[64];
  pages = (NUM_MODULES + pp - 1) / pp;
  if (page >= pages) page = pages - 1;
  if (page < 0) page = 0;
  first = page * pp;

  nbtn = 0;
  x = W - m;
  x -= label_w(exit_t, 90, 110); add_btn(x, m, W - m - x, tb - m, H_EXIT, exit_t);
  x -= gap + 60; add_btn(x, m, 60, tb - m, H_HELP, "?");
  x -= gap + label_w(rand_t, 150, 170); add_btn(x, m, label_w(rand_t, 150, 170), tb - m, H_RANDOM, rand_t);
  SetFont(ui_f_title, BLACK);
  ui_text_center(StringWidth(tr(&S_TITLE)) > x - gap - m ? ui_f_small : ui_f_title, BLACK, m, m, x - gap - m, tb - m, tr(&S_TITLE));
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);

  for (i = first; i < NUM_MODULES && i < first + pp; i++) {
    const Module *mod = MODULES[i];
    int idx = i - first, tx = m + (idx % cols) * (tile_w + gap), y = tb + 8 + (idx / cols) * (tile_h + 10);
    ui_box(tx, y, tile_w, tile_h, 3, WHITE);
    SetFont(ui_f_title, BLACK);
    DrawString(tx + 16, y + 12, tr(&mod->title));
    snprintf(t, sizeof t, tr(&S_SOLVED), mod->solved_count(), mod->puzzle_count());
    SetFont(ui_f_small, BLACK);
    DrawString(tx + tile_w - 16 - StringWidth(t), y + 14, t);
    DrawTextRect(tx + 16, y + 20 + ui_f_title->height, tile_w - 32, tile_h - 28 - ui_f_title->height, (char *)tr(&mod->description), ALIGN_LEFT);
    add_btn(tx, y, tile_w, tile_h, H_TILE + i, "");
  }

  add_btn(m, by, label_w(set_t, 200, 260), 62, H_SETTINGS, set_t);
  ui_draw_btn(&btns[nbtn - 1]);
  if (pages > 1) {
    add_btn(W - m - 2 * 100 - gap, by, 100, 62, H_PREV, "<");
    add_btn(W - m - 100, by, 100, 62, H_NEXT, ">");
    ui_draw_btn(&btns[nbtn - 2]);
    ui_draw_btn(&btns[nbtn - 1]);
    snprintf(t, sizeof t, "%d / %d", page + 1, pages);
    ui_text_center(ui_f_btn, BLACK, m + label_w(set_t, 200, 260), by, W - 2 * m - label_w(set_t, 200, 260) - 2 * 100 - gap, 62, t);
  }
}

static void draw_help(int W, int H) {
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_HELP_TITLE));
  SetFont(ui_f_small, BLACK);
  DrawTextRect(16, 64, W - 32, H - 80, (char *)tr(&S_HELP_TEXT), ALIGN_LEFT);
}

/* Settings: one section, Language, as radio rows (System default, Русский, English, Deutsch). */
static void draw_settings(int W, int H) {
  static const Str *const NAME[4] = {&S_LANG_SYSTEM, &S_LANG_RU, &S_LANG_EN, &S_LANG_DE};
  static const int VALUE[4] = {LANG_SYSTEM, LANG_RU, LANG_EN, LANG_DE};
  int m = 8, gap = 10, tb = 58, i, rh = (W > H) ? 66 : 84, y;
  const char *back_t = tr(&S_BACK);
  nbtn = 0;
  add_btn(W - m - label_w(back_t, 120, 150), m, label_w(back_t, 120, 150), tb - m, H_BACK, back_t);
  ui_text_center(ui_f_title, BLACK, m, m, W - 2 * m - label_w(back_t, 120, 150) - gap, tb - m, tr(&S_SETTINGS));
  ui_draw_btn(&btns[0]);

  SetFont(ui_f_btn, BLACK);
  DrawString(m + 8, tb + 16, tr(&S_LANGUAGE));
  y = tb + 16 + ui_f_btn->height + 10;
  for (i = 0; i < 4; i++, y += rh + gap) {
    int on = lang_setting() == VALUE[i], my = y + (rh - 26) / 2;
    ui_box(m, y, W - 2 * m, rh, on ? 4 : 2, on ? 0xDDDDDD : WHITE);
    DrawRect(m + 24, my, 26, 26, BLACK);
    DrawRect(m + 25, my + 1, 24, 24, BLACK);
    if (on) FillArea(m + 31, my + 7, 12, 12, BLACK);
    SetFont(on ? ui_f_btn : ui_f_small, BLACK);
    DrawString(m + 24 + 26 + 24, y + (rh - (on ? ui_f_btn : ui_f_small)->height) / 2, tr(NAME[i]));
    add_btn(m, y, W - 2 * m, rh, H_LANG + i, "");
  }
}

static void redraw_home(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts((W > H) ? 26 : 28);
  ClearScreen();
  if (home_mode == HM_HELP) draw_help(W, H);
  else if (home_mode == HM_SETTINGS) draw_settings(W, H);
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
  home_mode = HM_HOME;
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
  if (home_mode == HM_HELP) { home_mode = HM_HOME; redraw_home(1); return; }
  for (i = 0; i < nbtn; i++) {
    const UiBtn *b = &btns[i];
    if (!ui_hit(b, x, y)) continue;
    if (b->id >= H_TILE) open_module(MODULES[b->id - H_TILE], -1);
    else if (b->id >= H_LANG && b->id < H_LANG + 4) {
      static const int VALUE[4] = {LANG_SYSTEM, LANG_RU, LANG_EN, LANG_DE};
      lang_set_setting(VALUE[b->id - H_LANG]);
      ui_log("language setting %d -> %d", lang_setting(), lang(), 0);
      redraw_home(1);
    }
    else if (b->id == H_SETTINGS) { home_mode = HM_SETTINGS; redraw_home(1); }
    else if (b->id == H_BACK) { home_mode = HM_HOME; redraw_home(1); }
    else if (b->id == H_RANDOM) random_puzzle();
    else if (b->id == H_HELP) { home_mode = HM_HELP; redraw_home(1); }
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
      lang_load();
      for (i = 0; i < NUM_MODULES; i++) MODULES[i]->init();
      ui_log("init %dx%d, modules %d", ScreenWidth(), ScreenHeight(), NUM_MODULES);
      ui_log("language setting %d, showing %d (-1 system, 0 en, 1 ru, 2 de)", lang_setting(), lang(), 0);
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
      ui_log("key %d", par1, 0, 0);
      if (active) active->event(type, par1, par2);
      else if (par1 == KEY_BACK) {
        if (home_mode != HM_HOME) { home_mode = HM_HOME; redraw_home(1); }
        else CloseApp();
      } else if (par1 == KEY_MENU && home_mode == HM_HOME) {
        home_mode = HM_SETTINGS; redraw_home(1);
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
