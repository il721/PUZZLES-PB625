/* Template for a new Puzzle Book module. Copy this folder, rename, and replace the game.
 * Not registered in modules.c; tools/test-rebus-ui.sh builds it in a second variant to check
 * that the shell hosts more than one module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"

#ifndef SAVEF
#define SAVEF "/mnt/ext1/example.sav"
#endif

static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_TITLE = S("Example module", "Образец модуля", "Beispielmodul");
static const Str S_TAP_DONE = S("Done! Tap to reset.", "Готово! Нажмите, чтобы сбросить.", "Fertig! Tippen zum Zurücksetzen.");
static const Str S_TAP = S("Tap anywhere to finish.", "Нажмите в любом месте, чтобы закончить.", "Tippen Sie irgendwo, um fertig zu werden.");

static int done;
static UiBtn home_btn;

static void save(void) {
  FILE *f = fopen(SAVEF, "w");
  if (!f) return;
  fprintf(f, "%d\n", done);
  fclose(f);
}

static void init(void) {
  char line[16];
  FILE *f = fopen(SAVEF, "r");
  if (!f) return;
  /* atoi, not fscanf/sscanf: those need GLIBC_2.7 and the firmware's libc may be older */
  done = fgets(line, sizeof line, f) ? atoi(line) : 0;
  fclose(f);
}

static void draw(void) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts((W > H) ? 26 : 28);
  ClearScreen();
  home_btn.x = W - 98; home_btn.y = 8; home_btn.w = 90; home_btn.h = 50; home_btn.id = 1; home_btn.label = tr(&S_HOME);
  ui_draw_btn(&home_btn);
  ui_text_center(ui_f_title, BLACK, 0, 8, W - 100, 50, tr(&S_TITLE));
  ui_text_center(ui_f_btn, BLACK, 0, H / 2 - 30, W, 60, tr(done ? &S_TAP_DONE : &S_TAP));
  FullUpdate();
}

static void enter(int puzzle) { (void)puzzle; draw(); }

static int event(int type, int par1, int par2) {
  switch (type) {
    case EVT_SHOW:
    case EVT_ORIENTATION:
      draw();
      break;
    case EVT_POINTERUP:
      if (ui_hit(&home_btn, par1, par2)) { shell_leave(); break; }
      done = !done;
      save();
      draw();
      break;
    case EVT_KEYPRESS:
      if (par1 == KEY_BACK) shell_leave();
      break;
    default:
      break;
  }
  (void)par2;
  return 0;
}

static int count(void) { return 1; }
static int solved(void) { return done; }
static int pick_random(void) { return 0; }

const Module module_example = {
    "example", S("Example module", "Образец модуля", "Beispielmodul"),
    S("A template: copy native/puzzlebook/example to start a new game.",
      "Шаблон: скопируйте native/puzzlebook/example, чтобы начать новую игру.",
      "Eine Vorlage: native/puzzlebook/example kopieren, um ein neues Spiel zu beginnen."),
    init, enter, event, save, count, solved, pick_random,
};
