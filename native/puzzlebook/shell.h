/* Puzzle Book shell: the envelope that hosts puzzle modules (like the app shell in il721/puzzles).
 *
 * inkview gives an app one event handler, so the shell owns it. On the home screen it handles the
 * events itself; once a module is opened every event is forwarded to that module until the module
 * calls shell_leave().
 *
 * Adding a module (see README.md in this folder):
 *   1. implement a `const Module module_xxx` in a new folder (see rebus/module_rebus.c),
 *   2. add it to MODULES[] in modules.c,
 *   3. add its .c files to MODSRC in the Makefile.
 */
#ifndef SHELL_H
#define SHELL_H

#include "inkview.h"

typedef struct Module {
  const char *id;          /* short stable id; also the save file: /mnt/ext1/<id>.sav */
  const char *title;       /* shown on the home tile */
  const char *description; /* one or two sentences, shown on the tile and in the help */
  void (*init)(void);      /* once at start: build state, load the save file */
  void (*enter)(int puzzle); /* open the module: puzzle >= 0 opens that puzzle, -1 resumes. Must draw. */
  /* Events while the module is open (EVT_SHOW, EVT_ORIENTATION, EVT_POINTERUP, EVT_KEYPRESS).
   * The module redraws itself and calls shell_leave() to go back to the home screen. */
  int (*event)(int type, int par1, int par2);
  void (*save)(void);           /* write the save file (also called on exit) */
  int (*puzzle_count)(void);    /* playable puzzles, tutorials not counted */
  int (*solved_count)(void);
  int (*pick_random)(void);     /* index of a random unsolved puzzle (any if all are solved) */
} Module;

extern const Module *const MODULES[];
extern const int NUM_MODULES;

/* Called by a module to return to the home screen. */
void shell_leave(void);

/* ---- shared drawing helpers (ui.c) ---- */
typedef struct { int x, y, w, h, id; const char *label; } UiBtn;

extern ifont *ui_f_btn, *ui_f_small, *ui_f_title; /* set by ui_fonts() */
void ui_fonts(int btn_size);
ifont *ui_open_font(const char *name, int size);
void ui_text_center(ifont *f, int color, int x, int y, int w, int h, const char *s);
void ui_box(int x, int y, int w, int h, int thick, int fill);
void ui_draw_btn(const UiBtn *b);
int ui_hit(const UiBtn *b, int x, int y);
void ui_log(const char *fmt, int a, int b, int c); /* appends to /mnt/ext1/puzzlebook.txt */

#endif
