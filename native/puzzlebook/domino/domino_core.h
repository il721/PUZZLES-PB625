/* Domino Solitaire: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_domino (il721/puzzles): an 8x7 grid of printed digits 0..6 (56 cells) that the player
 * splits into 28 dominoes forming one complete double-six set (0:0 .. 6:6, every value pair exactly once).
 * Tapping a cell and then an adjacent free cell binds them into a domino; tapping a placed domino removes it.
 * Win = every cell covered and no value pair used twice (any such tiling wins, not only the book's one).
 */
#ifndef DOMINO_CORE_H
#define DOMINO_CORE_H

#include <stdint.h>

#define DROWS 8
#define DCOLS 7
#define DCELLS (DROWS * DCOLS)
#define DDOMS (DCELLS / 2)

typedef struct {
  const char *id;
  int tutorial;
  int8_t grid[DROWS][DCOLS];
  int nsol;                  /* 28 for the sample (the book's printed solution), else 0 */
  int8_t sol[DDOMS][4];      /* r1, c1, r2, c2 per domino */
} DomPuzzle;

typedef struct {
  const DomPuzzle *p;
  int8_t partner[DCELLS]; /* index (row * 7 + col) of the other half of the domino covering the cell, -1 = free */
  int8_t pending;         /* the cell tapped once, awaiting its partner, -1 = none */
} DomBoard;

typedef struct {
  int placed;              /* dominoes on the board */
  int covered;             /* covered cells */
  int count[7][7];         /* placed dominoes per value pair [low][high] */
  uint8_t conflict[DCELLS]; /* 1 = the cell belongs to a domino whose value pair is used more than once */
  int nconflicts;          /* cells with conflict set */
  int solved;              /* all 56 cells covered and no conflict */
} DomStatus;

void dom_init(DomBoard *b, const DomPuzzle *p);
void dom_reset(DomBoard *b);
/* One tap, the source's state machine: covered cell -> remove its domino; free cell -> select / deselect / bind with
 * an adjacent selected cell / move the selection. Returns 1 if the placed dominoes changed. */
int dom_tap(DomBoard *b, int r, int c);
/* Places a domino on two free adjacent in-range cells; 0 (and nothing changes) otherwise. */
int dom_place(DomBoard *b, int r1, int c1, int r2, int c2);
void dom_status(const DomBoard *b, DomStatus *st);
int dom_wins(const DomBoard *b);
/* Dominoes in reading order of their first cell: fills out[k] = {r1, c1, r2, c2}; returns the count. */
int dom_list(const DomBoard *b, uint8_t out[DDOMS][4]);

/* Save format: 56 chars in reading order: '.' free or second half, 'R' = first half of a horizontal domino,
 * 'D' = first half of a vertical one. Buffer needs DOM_SAVE_LEN + 1 bytes. */
#define DOM_SAVE_LEN DCELLS
void dom_save(const DomBoard *b, char *out);
void dom_load(DomBoard *b, const char *s); /* tolerant: bad chars, overlaps and edge overruns are skipped */

#endif
