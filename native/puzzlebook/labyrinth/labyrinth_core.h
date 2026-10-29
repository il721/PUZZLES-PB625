/* Alphabet Labyrinth: puzzle model and rules (no inkview, builds on any host).
 *
 * Port of packages/module_labyrinth (il721/puzzles): an 8x8 grid of the 33 letters of the Russian alphabet (each
 * letter once or twice). The player threads one self-avoiding orthogonal path of 33 cells from the top-left cell (А)
 * to the bottom-right one (Я) that visits every letter of the alphabet exactly once. The path grows from both ends
 * (chain A from the top-left, chain Z from the bottom-right) and is closed when the two heads touch. Off the path
 * the player may mark cells "must be on the path" (marks) or cross them out (crosses); a letter that is already on
 * the path crosses out its twins automatically. Win = joined, 33 cells, no letter twice (any such path wins).
 * Cells are indices row * 8 + col.
 */
#ifndef LABYRINTH_CORE_H
#define LABYRINTH_CORE_H

#include <stdint.h>

#define LCELLS 64
#define LPATH 33
#define LSTART 0
#define LEND 63

typedef struct {
  const char *id;
  int tutorial;
  int8_t letter[LCELLS]; /* 0..32: index into the Russian alphabet */
  int nsol;              /* 33 when the book's path is recorded */
  int8_t sol[LPATH];     /* cells of the path, А to Я */
} LabPuzzle;

typedef struct {
  const LabPuzzle *p;
  int8_t a[LPATH], z[LPATH]; /* chain A / chain Z: cell indices, element 0 is the anchor (0 / 63) */
  int na, nz;
  uint8_t cross[LCELLS]; /* manual crosses */
  uint8_t mark[LCELLS];  /* "must be on the path" marks */
} LabBoard;

typedef struct {
  int joined;            /* the two heads are orthogonally adjacent */
  int placed;            /* cells on the path (na + nz) */
  int used[33];          /* occurrences of each letter on the path */
  int ndup;              /* letters occurring more than once */
  int nmissing;          /* letters not on the path */
  uint8_t auto_cross[LCELLS]; /* off-path cells whose letter is already on the path */
  int solved;
} LabStatus;

void lab_init(LabBoard *b, const LabPuzzle *p);
void lab_reset(LabBoard *b);
int lab_on_path(const LabBoard *b, int cell);
int lab_joined(const LabBoard *b);
/* One tap, the source's state machine: a head retracts; a joined path ignores the rest; an adjacent free cell extends
 * chain A (tie: A) or chain Z; any other cell toggles its mark. Returns 1 if anything changed. */
int lab_tap(LabBoard *b, int cell);
/* The source's long press: a chain cell (not an anchor) cuts that chain back to just before it; an anchor does
 * nothing; any other cell toggles its manual cross. Returns 1 if anything changed. */
int lab_cross(LabBoard *b, int cell);
void lab_status(const LabBoard *b, LabStatus *st);
int lab_wins(const LabBoard *b);
/* Fills out with chain A followed by chain Z reversed; returns the count. */
int lab_path(const LabBoard *b, int8_t out[LPATH]);

/* Restores a board from lists of cells; returns 0 and leaves the board fresh if anything is wrong (wrong anchors,
 * non-adjacent steps, repeated or shared cells, too long, out of range), like the source's fromJson. */
int lab_restore(LabBoard *b, const uint8_t *a, int na, const uint8_t *z, int nz, const uint8_t *x, int nx, const uint8_t *m, int nm);

/* Save format: four fields (chain A, chain Z, crosses, marks), each one char per cell ('0' + index), "-" if empty.
 * Buffer needs LAB_SAVE_LEN bytes. */
#define LAB_SAVE_LEN (LPATH * 2 + LCELLS * 2 + 8)
void lab_save(const LabBoard *b, char *out);
int lab_load(LabBoard *b, const char *a, const char *z, const char *x, const char *m); /* the fields; 0 = bad, board fresh */

#endif
