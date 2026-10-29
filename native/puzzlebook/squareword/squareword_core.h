/* Squarewords: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_squareword (il721/puzzles): an n x n grid (n = 5, 6 or 7) built around an n-letter
 * Cyrillic keyword. Row 0 is the keyword; a few more cells are printed (givens). The player fills the rest so that
 * every row, every column and both main diagonals hold each keyword letter exactly once. A letter that repeats in
 * a line is flagged, never refused. Win = grid full and nothing flagged (any such grid wins, not only the book's).
 */
#ifndef SQUAREWORD_CORE_H
#define SQUAREWORD_CORE_H

#include <stdint.h>

#define SW_MAX 7
#define SW_CELLS (SW_MAX * SW_MAX)

typedef struct {
  const char *id;
  int tutorial;
  int n;
  uint16_t kw[SW_MAX];       /* the keyword's letters as Unicode code points, in keyword order (letter k = kw[k]) */
  int8_t given[SW_MAX][SW_MAX]; /* letter index of a printed cell, -1 = player cell */
} SwPuzzle;

typedef struct {
  const SwPuzzle *p;
  int8_t cell[SW_MAX][SW_MAX]; /* letter index in the cell, -1 = empty (givens are filled from the puzzle) */
  int8_t sel_r, sel_c;         /* selected player cell, sel_r = -1 for none */
} SwBoard;

typedef struct {
  int filled;                  /* cells holding a letter, givens included */
  int violations;              /* cells that take part in a repeat */
  uint8_t bad[SW_MAX][SW_MAX]; /* 1 = the cell repeats a letter in its row, column or diagonal (both cells are flagged) */
  int count[SW_MAX];           /* how often each keyword letter is on the board */
  int solved;                  /* every cell filled and nothing flagged */
} SwStatus;

void sw_init(SwBoard *b, const SwPuzzle *p);
void sw_reset(SwBoard *b);      /* back to the printed cells only; clears the selection */
int sw_is_given(const SwBoard *b, int r, int c);
/* Sets a player cell to letter (0..n-1), or empties it for letter = -1. 0 (and nothing changes) for givens,
 * out-of-range cells and letters; 1 otherwise. */
int sw_set(SwBoard *b, int r, int c, int letter);
void sw_status(const SwBoard *b, SwStatus *st);
int sw_wins(const SwBoard *b);
/* Player cells that hold a letter, reading order: out[k] = {row, col, letter}; returns the count. */
int sw_list(const SwBoard *b, uint8_t out[SW_CELLS][3]);

/* Save format: n*n chars in reading order: '.' empty or given, else the letter index as '0'..'6'. Buffer needs
 * SW_SAVE_LEN + 1 bytes. */
#define SW_SAVE_LEN SW_CELLS
void sw_save(const SwBoard *b, char *out);
void sw_load(SwBoard *b, const char *s); /* tolerant: bad chars and cells that are given are skipped */

#endif
