/* Keyword Rebuses: the 10 steps of the guided walkthrough of the "Example" puzzle ex1 (ПЗ x А = ПЕП, УУ + У = ЗТ,
 * ИГЕ + НО = ИНЗ; tutorial_steps.dart of module_keyword_rebus in il721/puzzles; texts in ../tutorial_text.c). Builds on any
 * host, no drawing.
 *
 * A step gives letters their digits (cumulative over the steps: the digit is the book's, KwPuzzle.ans) and emphasises words of
 * the 3x3 grid, as {row, col}. The deduction chain is И, П, Г, then У / З / Т, Е, О, Н, А; the ten letters in the order of
 * their digits spell ГИПОТЕНУЗА. Letters are Unicode code points. */
#ifndef KEYWORD_TUTORIAL_H
#define KEYWORD_TUTORIAL_H

#include <string.h>

#include "keyword_core.h"

#define KT_TUT_STEPS 10

typedef struct {
  int8_t nrev;
  uint16_t rev[3];       /* letters that get their digit in this step */
  int8_t nword;
  int8_t word[5][2];     /* emphasised words {row, col} */
} KtTutStep;

#define K_I 0x418
#define K_P 0x41F
#define K_G 0x413
#define K_U 0x423
#define K_Z 0x417
#define K_T 0x422
#define K_E 0x415
#define K_O 0x41E
#define K_N 0x41D
#define K_A 0x410

static const KtTutStep KT_TUT[KT_TUT_STEPS] = {
    /* 1 */ {0, {0}, 0, {{0}}},
    /* 2 */ {1, {K_I}, 3, {{0, 0}, {1, 0}, {2, 0}}},
    /* 3 */ {1, {K_P}, 3, {{0, 2}, {1, 2}, {2, 2}}},
    /* 4 */ {1, {K_G}, 3, {{2, 0}, {2, 1}, {2, 2}}},
    /* 5 */ {3, {K_U, K_Z, K_T}, 5, {{1, 0}, {1, 1}, {1, 2}, {0, 0}, {2, 0}}},
    /* 6 */ {1, {K_E}, 3, {{0, 0}, {1, 0}, {2, 0}}},
    /* 7 */ {1, {K_O}, 3, {{2, 0}, {2, 1}, {2, 2}}},
    /* 8 */ {1, {K_N}, 3, {{0, 2}, {1, 2}, {2, 2}}},
    /* 9 */ {1, {K_A}, 3, {{0, 1}, {1, 1}, {2, 1}}},
    /* 10 */ {0, {0}, 0, {{0}}},
};

/* Index of the puzzle letter with code point cp, -1 if there is none. */
static int kt_tut_letter(const KwPuzzle *p, unsigned cp) {
  int k;
  for (k = 0; k < KW_LETTERS; k++)
    if (p->let[k] == cp) return k;
  return -1;
}

/* The board of step `step` (0-based): every letter that has its digit up to and including that step; emph[row][col] = 1 for
 * the words the step emphasises. */
static void kt_tut_board(const KwPuzzle *p, int step, KwBoard *b, uint8_t emph[3][3]) {
  int i, j;
  kw_init(b, p);
  memset(emph, 0, 9);
  for (i = 0; i <= step && i < KT_TUT_STEPS; i++)
    for (j = 0; j < KT_TUT[i].nrev; j++) {
      int l = kt_tut_letter(p, KT_TUT[i].rev[j]);
      if (l >= 0) kw_set(b, l, p->ans[l]);
    }
  for (j = 0; j < KT_TUT[step].nword; j++) emph[(int)KT_TUT[step].word[j][0]][(int)KT_TUT[step].word[j][1]] = 1;
}

#endif
