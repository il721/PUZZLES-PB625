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
#include "i18n.h"

/* One puzzle's progress, the unit of the save/load-to-file feature (see progress.c). Cell keys and the entry
 * layout are those of il721/puzzles ("cells": {"row:slot:pos": digit} for Square Rebuses, {"row:col": digit} for
 * Digit Rebuses, "placed": [[[r,c],[r,c]], ...] for Domino Solitaire, "chainA"/"chainZ"/"crosses"/"marks": [[r,c], ...]
 * for Alphabet Labyrinth, "filled": {"r,c": "letter"} for Squarewords, "assignment": {"letter": digit} for Keyword Rebuses), so a progress file can move between the
 * reader and the app. progress.c clears the entry before every prog_get, so a module only sets what it uses. */
#define PROG_MAX_CELLS 80
#define PROG_MAX_DOMS 28
#define PROG_LISTS 4
#define PROG_MAX_LIST 64
#define PROG_MAX_FILL 49
#define PROG_MAX_ASG 10
typedef struct { char key[12]; int digit; } ProgCell;
typedef struct {
  int solved;
  long long updated_at; /* ms since 1970 of the last change, 0 = unknown */
  int ncells;
  ProgCell cells[PROG_MAX_CELLS];
  int ndom;             /* dominoes ("placed"), each {r1, c1, r2, c2} */
  unsigned char dom[PROG_MAX_DOMS][4];
  int has_lists;        /* cell lists: list[0..3] = chainA, chainZ, crosses, marks; each cell is {row, col} */
  int nlist[PROG_LISTS];
  unsigned char list[PROG_LISTS][PROG_MAX_LIST][2];
  int has_fill;         /* letters ("filled": {"row,col": "Л"}) for Squarewords: fill[k] = {row, col}, fill_cp[k] = code point */
  int nfill;
  unsigned char fill[PROG_MAX_FILL][2];
  unsigned short fill_cp[PROG_MAX_FILL];
  int has_asg;          /* letter -> digit ("assignment": {"Л": 3}) for Keyword Rebuses: asg_cp[k] = code point, asg_digit[k] = 0..9 */
  int nasg;
  int asg_bad;          /* the file's assignment had a non-digit value or more than ten letters: the module rejects the entry */
  unsigned short asg_cp[PROG_MAX_ASG];
  unsigned char asg_digit[PROG_MAX_ASG];
} ProgEntry;

typedef struct Module {
  const char *id;          /* short stable id; also the save file: /mnt/ext1/<id>.sav */
  Str title;               /* shown on the home tile (all languages, see i18n.h) */
  Str description;         /* one or two sentences, shown on the tile */
  void (*init)(void);      /* once at start: build state, load the save file */
  void (*enter)(int puzzle); /* open the module: puzzle >= 0 opens that puzzle, -1 resumes. Must draw. */
  /* Events while the module is open (EVT_SHOW, EVT_ORIENTATION, EVT_POINTERUP, EVT_KEYPRESS).
   * The module redraws itself and calls shell_leave() to go back to the home screen. */
  int (*event)(int type, int par1, int par2);
  void (*save)(void);           /* write the save file (also called on exit) */
  int (*puzzle_count)(void);    /* playable puzzles, tutorials not counted */
  int (*solved_count)(void);
  int (*pick_random)(void);     /* index of a random unsolved puzzle (any if all are solved) */
  /* Progress file (Settings > Transfer progress). A module without ns is left out of the file. */
  const char *ns;                                /* namespace in the file, as in the app: "module_rebus" */
  int (*prog_count)(void);                       /* puzzles including the sample: indices 0..n-1 */
  const char *(*prog_id)(int i);                 /* puzzle id as in the app's data: "p01", "example" */
  int (*prog_get)(int i, ProgEntry *e);          /* fills e; 0 if the puzzle was never touched */
  void (*prog_put)(int i, const ProgEntry *e);   /* replaces puzzle i's progress with e */
} Module;

extern const Module *const MODULES[];
extern const int NUM_MODULES;

/* Called by a module to return to the home screen. */
void shell_leave(void);

/* ---- shared drawing helpers (ui.c) ---- */
typedef struct { int x, y, w, h, id; const char *label; } UiBtn;

extern ifont *ui_f_btn, *ui_f_small, *ui_f_title, *ui_f_tiny; /* set by ui_fonts() */
void ui_fonts(int btn_size);
ifont *ui_open_font(const char *name, int size);
void ui_text_center(ifont *f, int color, int x, int y, int w, int h, const char *s);
void ui_box(int x, int y, int w, int h, int thick, int fill);
void ui_draw_btn(const UiBtn *b); /* label shrinks (btn -> small -> tiny font) until it fits: Russian/German words are long */
int ui_hit(const UiBtn *b, int x, int y);
void ui_log(const char *fmt, int a, int b, int c); /* appends to /mnt/ext1/puzzlebook.txt */
void ui_utf8(unsigned cp, char *buf);   /* code point to UTF-8, buf needs 4 bytes */
unsigned ui_cp(const char *s);          /* first UTF-8 character to code point, 0 if malformed */
long long ui_now_ms(void);                          /* ms since 1970 (stamps for ProgEntry.updated_at) */

#endif
