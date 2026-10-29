/* Digit Rebuses: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_digit_rebus (il721/puzzles): a 4x4 square of digit cells, each masked by one of
 * five glyphs that admits only a few digits. Every row and every column is an equation
 * "c1 op c2 op c3 = c4", evaluated strictly left to right; every intermediate value must be a
 * non-negative integer (a step below zero, a division by zero or an inexact division breaks the line).
 * There are no given digits. Win = any complete grid that satisfies all rules (not canonical match).
 */
#ifndef DIGIT_CORE_H
#define DIGIT_CORE_H

#include <stdint.h>

typedef struct {
  const char *id;
  int tutorial;
  int8_t glyph[4][4];       /* 1..5 = G1..G5 */
  const char *rowops[4];    /* two operators per row, out of + - * : */
  const char *colops[4];    /* two operators per column */
  int8_t answer[4][4];      /* the book's printed solution (used by the tests only) */
} DPuzzle;

typedef struct {
  const DPuzzle *p;
  int8_t d[4][4]; /* -1 = empty */
} DGrid;

/* Cells that take part in a broken rule. */
typedef struct {
  uint8_t bad[4][4];
  int count; /* number of broken rules (glyph, row or column) */
} DMarks;

/* Whether glyph g (1..5) admits digit d. */
int glyph_admits(int g, int d);

void dgrid_init(DGrid *g, const DPuzzle *p);
/* Sets any digit 0..9 (glyph membership is checked by dgrid_violations, like in the source). 1 = changed. */
int dgrid_set(DGrid *g, int r, int c, int digit);
int dgrid_clear(DGrid *g, int r, int c);
void dgrid_reset(DGrid *g);
int dgrid_complete(const DGrid *g);
/* Fills m; returns m->count. Equations fire only once all four of their cells are filled. */
int dgrid_violations(const DGrid *g, DMarks *m);
int dgrid_wins(const DGrid *g);

/* Evaluates cells[0] ops[0] cells[1] ops[1] cells[2] left to right; -1 if invalid. */
int deval_line(const int cells[3], const char *ops);

/* Save format: 16 chars in reading order ('.' = empty, '0'..'9'). Buffer needs DGRID_SAVE_LEN + 1 bytes. */
#define DGRID_SAVE_LEN 16
void dgrid_save(const DGrid *g, char *out);
void dgrid_load(DGrid *g, const char *s); /* ignores bad chars */

#endif
