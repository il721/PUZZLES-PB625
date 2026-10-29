/* Square Rebuses: the 15 steps of the guided walkthrough of the "Example" puzzle (tutorial_steps.dart of module_rebus in
 * il721/puzzles; the texts are in ../tutorial_text.c). Builds on any host, no drawing.
 *
 * A step reveals cells (cumulative over the steps: the cell gets the digit the sample's printed numbers have) and emphasises
 * a whole grid row (0..4, 4 = the summary row), a whole column (operand slot 0..3 of rows 0..3 plus the summary cell of
 * that column) and/or single cells. Cells are {row, slot, pos} as in rebus_core.h. */
#ifndef REBUS_TUTORIAL_H
#define REBUS_TUTORIAL_H

#include <string.h>

#include "rebus_core.h"

#define RB_TUT_STEPS 15

typedef struct {
  int8_t hrow, hcol;     /* emphasised row / column, -1 = none */
  int8_t nrev;
  int8_t rev[8][3];      /* cells revealed by this step */
  int8_t ncell;
  int8_t cell[4][3];     /* single cells emphasised */
} RbTutStep;

static const RbTutStep RB_TUT[RB_TUT_STEPS] = {
    /* 1 */ {-1, -1, 0, {{0}}, 0, {{0}}},
    /* 2 */ {0, -1, 0, {{0}}, 0, {{0}}},
    /* 3 */ {4, -1, 0, {{0}}, 0, {{0}}},
    /* 4 */ {-1, -1, 0, {{0}}, 4, {{0, 4, 0}, {4, 0, 0}, {4, 1, 1}, {1, 4, 1}}},
    /* 5 */ {1, -1, 1, {{1, 2, 0}}, 0, {{0}}},
    /* 6 */ {-1, -1, 0, {{0}}, 2, {{1, 4, 0}, {1, 4, 1}}},
    /* 7 */ {1, -1, 5, {{1, 0, 0}, {1, 1, 0}, {1, 3, 0}, {1, 4, 0}, {4, 1, 0}}, 0, {{0}}},
    /* 8 */ {-1, 1, 0, {{0}}, 0, {{0}}},
    /* 9 */ {-1, 1, 2, {{2, 1, 0}, {3, 1, 0}}, 0, {{0}}},
    /* 10 */ {-1, -1, 0, {{0}}, 4, {{0, 4, 0}, {0, 4, 1}, {4, 0, 0}, {4, 0, 1}}},
    /* 11 */ {2, -1, 5, {{2, 0, 0}, {2, 4, 0}, {2, 4, 1}, {4, 2, 0}, {4, 2, 1}}, 0, {{0}}},
    /* 12 */ {-1, 2, 0, {{0}}, 0, {{0}}},
    /* 13 */ {0, -1, 4, {{0, 0, 0}, {0, 2, 0}, {0, 4, 1}, {4, 0, 1}}, 0, {{0}}},
    /* 14 */ {3, -1, 8, {{3, 0, 0}, {3, 2, 0}, {3, 3, 0}, {3, 3, 1}, {3, 4, 0}, {3, 4, 1}, {4, 3, 0}, {4, 3, 1}}, 0, {{0}}},
    /* 15 */ {4, -1, 3, {{4, 4, 0}, {4, 4, 1}, {4, 4, 2}}, 0, {{0}}},
};

/* The board of step `step` (0-based): the puzzle's givens plus everything revealed up to and including that step, and
 * emph[row][slot][pos] = 1 for the cells the step emphasises. */
static void rb_tut_board(const Puzzle *p, int step, Grid *g, uint8_t emph[5][5][MAXPOS]) {
  int i, j, r, s, k;
  grid_init(g, p);
  memset(emph, 0, 5 * 5 * MAXPOS);
  for (i = 0; i <= step && i < RB_TUT_STEPS; i++)
    for (j = 0; j < RB_TUT[i].nrev; j++) {
      const int8_t *c = RB_TUT[i].rev[j];
      grid_set(g, c[0], c[1], c[2], puzzle_number(p, c[0], c[1])[c[2]] - '0');
    }
  {
    const RbTutStep *t = &RB_TUT[step];
    if (t->hrow >= 0)
      for (s = 0; s < 5; s++)
        for (k = 0; k < g->width[t->hrow][s]; k++) emph[t->hrow][s][k] = 1;
    if (t->hcol >= 0) {
      for (r = 0; r < 4; r++)
        for (k = 0; k < g->width[r][t->hcol]; k++) emph[r][t->hcol][k] = 1;
      for (k = 0; k < g->width[4][t->hcol]; k++) emph[4][t->hcol][k] = 1;
    }
    for (j = 0; j < t->ncell; j++) emph[t->cell[j][0]][t->cell[j][1]][t->cell[j][2]] = 1;
  }
}

#endif
