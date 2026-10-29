/* Domino Solitaire: the 10 steps of the guided walkthrough of the "Example" puzzle (tutorial_steps.dart of module_domino in
 * il721/puzzles; texts in ../tutorial_text.c). Builds on any host, no drawing.
 *
 * A step places dominoes of the sample's printed solution (indices into DomPuzzle.sol, the book's deduction order;
 * cumulative over the steps) and emphasises the ones it has just placed. */
#ifndef DOMINO_TUTORIAL_H
#define DOMINO_TUTORIAL_H

#include <string.h>

#include "domino_core.h"

#define DM_TUT_STEPS 10

typedef struct {
  int8_t nrev;
  int8_t rev[5];         /* solution indices placed (and emphasised) by this step */
} DmTutStep;

static const DmTutStep DM_TUT[DM_TUT_STEPS] = {
    /* 1 */ {0, {0}},
    /* 2 */ {0, {0}},
    /* 3 */ {3, {0, 1, 2}},
    /* 4 */ {2, {3, 4}},
    /* 5 */ {2, {5, 6}},
    /* 6 */ {4, {7, 8, 9, 10}},
    /* 7 */ {3, {11, 12, 13}},
    /* 8 */ {4, {14, 15, 16, 17}},
    /* 9 */ {5, {18, 19, 20, 21, 22}},
    /* 10 */ {5, {23, 24, 25, 26, 27}},
};

/* The board of step `step` (0-based): every domino placed up to and including that step, and emph[cell] = 1 for the cells of
 * the dominoes this step has just placed (cell = row * 7 + col). */
static void dm_tut_board(const DomPuzzle *p, int step, DomBoard *b, uint8_t emph[DCELLS]) {
  int i, j;
  dom_init(b, p);
  memset(emph, 0, DCELLS);
  for (i = 0; i <= step && i < DM_TUT_STEPS; i++)
    for (j = 0; j < DM_TUT[i].nrev; j++) {
      const int8_t *d = p->sol[(int)DM_TUT[i].rev[j]];
      dom_place(b, d[0], d[1], d[2], d[3]);
      if (i == step) emph[d[0] * DCOLS + d[1]] = emph[d[2] * DCOLS + d[3]] = 1;
    }
}

#endif
