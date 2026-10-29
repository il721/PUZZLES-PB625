/* Square Rebuses: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_rebus (il721/puzzles): four rows "a op b op c op d = r",
 * evaluated strictly left to right; column j sums to the result of row j; a
 * summary row repeats the four results and shows their total.
 *
 * Cell coordinates: row 0..3 = arithmetic row, slot 0..3 = operand, slot 4 =
 * result; row 4 = summary row, slot 0..3 = column sum (= result of that row),
 * slot 4 = grand total. pos 0 = most significant digit.
 */
#ifndef REBUS_CORE_H
#define REBUS_CORE_H

#include <stdint.h>

#define MAXPOS 3
#define MAXGIVENS 24

typedef struct {
  const char *nums[4];
  const char *ops; /* 3 chars out of + - * : */
  const char *result;
} PuzzleRow;

typedef struct {
  const char *id;
  int tutorial;
  PuzzleRow rows[4];
  const char *total;
  int ngivens;
  int givens[MAXGIVENS][4]; /* row, slot, pos, digit */
} Puzzle;

typedef struct {
  const Puzzle *p;
  int8_t digit[5][5][MAXPOS]; /* -1 = empty */
  uint8_t given[5][5][MAXPOS];
  uint8_t width[5][5];
} Grid;

/* Marks cells that take part in a broken rule (index [row][slot][pos]). */
typedef struct {
  uint8_t bad[5][5][MAXPOS];
  int count; /* number of broken rules */
} Marks;

/* Canonical string of the number at (row, slot). */
const char *puzzle_number(const Puzzle *p, int row, int slot);

void grid_init(Grid *g, const Puzzle *p);
/* No-op on givens. Return 1 if the cell was changed. */
int grid_set(Grid *g, int row, int slot, int pos, int digit);
int grid_clear(Grid *g, int row, int slot, int pos);
/* Clear every non-given cell. */
void grid_reset(Grid *g);
int grid_complete(const Grid *g);
/* Fills m; returns m->count. Rules only fire once all their cells are filled. */
int grid_violations(const Grid *g, Marks *m);
/* Complete and no violations (constraint satisfaction, not canonical match). */
int grid_wins(const Grid *g);

/* Save format: one char per cell in order row, slot, pos ('.' = empty, '0'..'9'),
 * givens included. Buffer needs GRID_SAVE_LEN + 1 bytes. */
#define GRID_SAVE_LEN (5 * 5 * MAXPOS)
void grid_save(const Grid *g, char *out);
/* Loads only non-given cells; ignores bad chars. */
void grid_load(Grid *g, const char *s);

/* Left-to-right evaluation. Returns 0 and sets *out, or -1 if invalid
 * (division by zero or inexact). */
int eval_row(const long nums[4], const char *ops, long *out);

#endif
