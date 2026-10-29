/* Squarewords: the 12 steps of the guided walkthrough of the "Example" puzzle (keyword СЛЕЗА, 5x5; tutorial_steps.dart of
 * module_squareword in il721/puzzles; texts in ../tutorial_text.c). Builds on any host, no drawing.
 *
 * Steps 1, 2 and 12 only talk; steps 3-11 replay the book's chain of eliminations cell by cell. A step places letters
 * (cumulative over the steps) and emphasises the cells it has just filled. Cells are {row, col} with row 0 the keyword row;
 * the letter is a Unicode code point (all Cyrillic: А U+0410, Е U+0415, З U+0417, Л U+041B, С U+0421). */
#ifndef SQUAREWORD_TUTORIAL_H
#define SQUAREWORD_TUTORIAL_H

#include <string.h>

#include "squareword_core.h"

#define SQ_TUT_STEPS 12

typedef struct { int8_t r, c; uint16_t cp; } SqRev;
typedef struct {
  int8_t nrev;
  SqRev rev[7];          /* cells filled by this step */
} SqTutStep;

#define L_A 0x410
#define L_E 0x415
#define L_Z 0x417
#define L_L 0x41B
#define L_S 0x421

static const SqTutStep SQ_TUT[SQ_TUT_STEPS] = {
    /* 1 */ {0, {{0, 0, 0}}},
    /* 2 */ {0, {{0, 0, 0}}},
    /* 3 */ {1, {{4, 3, L_L}}},
    /* 4 */ {2, {{3, 3, L_A}, {1, 3, L_S}}},
    /* 5 */ {1, {{4, 1, L_S}}},
    /* 6 */ {1, {{3, 2, L_S}}},
    /* 7 */ {1, {{4, 2, L_A}}},
    /* 8 */ {1, {{1, 2, L_Z}}},
    /* 9 */ {2, {{1, 0, L_A}, {2, 1, L_A}}},
    /* 10 */ {1, {{2, 0, L_Z}}},
    /* 11 */ {7, {{1, 1, L_E}, {1, 4, L_L}, {4, 4, L_Z}, {4, 0, L_E}, {3, 4, L_E}, {3, 0, L_L}, {3, 1, L_Z}}},
    /* 12 */ {0, {{0, 0, 0}}},
};

/* Index of the keyword letter with code point cp, -1 if there is none. */
static int sq_tut_letter(const SwPuzzle *p, unsigned cp) {
  int k;
  for (k = 0; k < p->n; k++)
    if (p->kw[k] == cp) return k;
  return -1;
}

/* The board of step `step` (0-based): the printed cells plus every letter placed up to and including that step;
 * emph[row][col] = 1 for the cells the step has just filled. */
static void sq_tut_board(const SwPuzzle *p, int step, SwBoard *b, uint8_t emph[SW_MAX][SW_MAX]) {
  int i, j;
  sw_init(b, p);
  memset(emph, 0, SW_MAX * SW_MAX);
  for (i = 0; i <= step && i < SQ_TUT_STEPS; i++)
    for (j = 0; j < SQ_TUT[i].nrev; j++) {
      const SqRev *e = &SQ_TUT[i].rev[j];
      sw_set(b, e->r, e->c, sq_tut_letter(p, e->cp));
      if (i == step) emph[e->r][e->c] = 1;
    }
}

#endif
