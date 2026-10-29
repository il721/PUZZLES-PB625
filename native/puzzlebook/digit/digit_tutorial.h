/* Digit Rebuses: the 9 steps of the guided walkthrough of the "Example" puzzle (tutorial_steps.dart of module_digit_rebus in
 * il721/puzzles, along the book's deduction chain: row 2, column 1, row 3, column 3, the rest; texts in ../tutorial_text.c).
 * Builds on any host, no drawing.
 *
 * A step reveals cells (cumulative over the steps: the cell gets the book's printed digit) and emphasises a whole row and/or
 * column (0..3). Cells are {row, col}. */
#ifndef DIGIT_TUTORIAL_H
#define DIGIT_TUTORIAL_H

#include <string.h>

#include "digit_core.h"

#define DG_TUT_STEPS 9

typedef struct {
  int8_t hrow, hcol;     /* emphasised row / column, -1 = none */
  int8_t nrev;
  int8_t rev[4][2];      /* cells revealed by this step */
} DgTutStep;

static const DgTutStep DG_TUT[DG_TUT_STEPS] = {
    /* 1 */ {-1, -1, 0, {{0}}},
    /* 2 */ {-1, -1, 0, {{0}}},
    /* 3 */ {1, -1, 0, {{0}}},
    /* 4 */ {1, -1, 4, {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    /* 5 */ {-1, 0, 3, {{0, 0}, {2, 0}, {3, 0}}},
    /* 6 */ {2, -1, 3, {{2, 1}, {2, 2}, {2, 3}}},
    /* 7 */ {-1, 2, 2, {{0, 2}, {3, 2}}},
    /* 8 */ {-1, -1, 4, {{0, 1}, {0, 3}, {3, 1}, {3, 3}}},
    /* 9 */ {-1, -1, 0, {{0}}},
};

/* The board of step `step` (0-based): everything revealed up to and including that step, and emph[row][col] = 1 for the
 * cells the step emphasises. */
static void dg_tut_board(const DPuzzle *p, int step, DGrid *g, uint8_t emph[4][4]) {
  int i, j, k;
  dgrid_init(g, p);
  memset(emph, 0, 16);
  for (i = 0; i <= step && i < DG_TUT_STEPS; i++)
    for (j = 0; j < DG_TUT[i].nrev; j++) dgrid_set(g, DG_TUT[i].rev[j][0], DG_TUT[i].rev[j][1], p->answer[DG_TUT[i].rev[j][0]][DG_TUT[i].rev[j][1]]);
  for (k = 0; k < 4; k++) {
    if (DG_TUT[step].hrow >= 0) emph[DG_TUT[step].hrow][k] = 1;
    if (DG_TUT[step].hcol >= 0) emph[k][DG_TUT[step].hcol] = 1;
  }
}

#endif
