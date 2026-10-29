/* Alphabet Labyrinth: the 12 steps of the guided walkthrough of the "Example" puzzle (tutorial_steps.dart of
 * module_labyrinth in il721/puzzles; texts in ../tutorial_text.c). Builds on any host, no drawing.
 *
 * Steps 1-3 teach the controls and show no path (step 3 shows two manual crosses on the twin Ю and a mark on У); steps 4-12
 * walk the printed solution in nine chunks (indices into LabPuzzle.sol: 0 = the cell of А, 32 = the cell of Я). Reveals are
 * cumulative: the revealed part grows from А as one chain, Я stays the anchor of the other. The step emphasises the cells of
 * its highlight range with a ring. */
#ifndef LABYRINTH_TUTORIAL_H
#define LABYRINTH_TUTORIAL_H

#include <string.h>

#include "labyrinth_core.h"

#define LB_TUT_STEPS 12

typedef struct {
  int8_t rlo, rhi;       /* solution indices revealed by this step, rlo = -1 for none */
  int8_t hlo, hhi;       /* solution indices emphasised, hlo = -1 for none */
  int8_t ncross;
  int8_t cross[2];       /* cells shown crossed out (this step only) */
  int8_t nmark;
  int8_t mark[1];        /* cells shown with the "must be on the path" mark (this step only) */
} LbTutStep;

static const LbTutStep LB_TUT[LB_TUT_STEPS] = {
    /* 1 */ {-1, -1, 0, 0, 0, {0}, 0, {0}},
    /* 2 */ {-1, -1, 32, 32, 0, {0}, 0, {0}},
    /* 3 */ {-1, -1, -1, -1, 2, {15, 16}, 1, {6}},
    /* 4 */ {0, 3, 0, 3, 0, {0}, 0, {0}},
    /* 5 */ {4, 7, 4, 7, 0, {0}, 0, {0}},
    /* 6 */ {8, 11, 8, 11, 0, {0}, 0, {0}},
    /* 7 */ {12, 15, 12, 15, 0, {0}, 0, {0}},
    /* 8 */ {16, 19, 16, 19, 0, {0}, 0, {0}},
    /* 9 */ {20, 23, 20, 23, 0, {0}, 0, {0}},
    /* 10 */ {24, 26, 24, 26, 0, {0}, 0, {0}},
    /* 11 */ {27, 29, 27, 29, 0, {0}, 0, {0}},
    /* 12 */ {30, 32, 30, 32, 0, {0}, 0, {0}},
};

/* The board of step `step` (0-based): the path revealed up to and including that step (A's chain gets solution cells 1..31 in
 * order; cell 32, the anchor of Я, is already there), plus this step's crosses and marks; emph[cell] = 1 for the cells the
 * step emphasises. */
static void lb_tut_board(const LabPuzzle *p, int step, LabBoard *b, uint8_t emph[LCELLS]) {
  int i, top = -1;
  const LbTutStep *t = &LB_TUT[step];
  lab_init(b, p);
  memset(emph, 0, LCELLS);
  for (i = 0; i <= step; i++)
    if (LB_TUT[i].rlo >= 0 && LB_TUT[i].rhi > top) top = LB_TUT[i].rhi;
  for (i = 1; i <= top && i < LPATH - 1; i++) lab_tap(b, p->sol[i]);
  for (i = 0; i < t->nmark; i++) lab_tap(b, t->mark[i]);
  for (i = 0; i < t->ncross; i++) lab_cross(b, t->cross[i]);
  if (t->hlo >= 0)
    for (i = t->hlo; i <= t->hhi; i++) emph[p->sol[i]] = 1;
}

#endif
