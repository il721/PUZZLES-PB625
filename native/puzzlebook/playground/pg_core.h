/* Puzzle games (the "Playground" module): the rules of all seven games, no drawing.
 * Port of il721/puzzles, packages/module_playground (engines of eight_chips, cats_dogs, hourglass, three_each, patterns5,
 * patterns4, swap_blocks). Three families:
 *   PGK_GRAPH  - tokens on the nodes of a line graph: eight chips and cats and dogs slide along lines, hourglass steps
 *                and jumps (a cascade of jumps is one move);
 *   PGK_TILES  - tiles laid in slots, each in one of four rotations: three of each (9 tiles) and the two patterns games;
 *   PGK_BLOCKS - swap blocks: twelve P-shaped blocks sliding through the free space of a 16 x 8 box.
 * One PgState holds the position of whichever game it was made for; a move is a PgMove (pg_apply validates it).
 * Undo and the save file replay a list of moves from pg_init. */
#ifndef PG_CORE_H
#define PG_CORE_H

enum { PG_EIGHT, PG_CATS, PG_HOUR, PG_THREE, PG_PAT5, PG_PAT4, PG_SWAP, PG_GAMES };
enum { PGK_GRAPH, PGK_TILES, PGK_BLOCKS };
enum { PG_MAXN = 29, PG_MAXSLOT = 25, PG_BLOCKS = 12, PG_SW_ROWS = 16, PG_SW_COLS = 8, PG_SW_CELLS = 128 };

typedef struct {
  signed char tok[PG_MAXN];       /* graph: 0 = free node, else the token (eight chips 1..8, cats 1 = cat 2 = dog, hourglass 1) */
  signed char tile[PG_MAXSLOT];   /* tiles: tile index lying in the slot, -1 = empty */
  signed char rot[PG_MAXSLOT];    /* tiles: quarter turns clockwise 0..3 */
  unsigned char org[PG_BLOCKS];   /* blocks: bounding-box origin of each block, row * 8 + col */
} PgState;

/* k: PGM_SLIDE a -> b (node indices; a step, a slide or a jump cascade)
 *    PGM_PLACE tile a into slot b at rotation c;  PGM_TURN the tile in slot a to rotation b;  PGM_TAKE the tile in slot a
 *    back to the tray;  PGM_BLOCK block a to origin b */
typedef struct { unsigned char k, a, b, c; } PgMove;
enum { PGM_SLIDE, PGM_PLACE, PGM_TURN, PGM_TAKE, PGM_BLOCK };

unsigned pg_pack(PgMove m);       /* a move as one number (the save file) */
PgMove pg_unpack(unsigned v);

int pg_kind(int g);               /* PGK_* */
const char *pg_id(int g);         /* the source's game id: "eight_chips" ... */
int pg_par(int g);                /* proven minimum number of moves, 0 = the game has none */

void pg_init(int g, PgState *s);                   /* the start position */
int pg_solved(int g, const PgState *s);
int pg_apply(int g, PgState *s, PgMove m);         /* 1 and applied if m is legal, else 0 and s unchanged */
int pg_replay(int g, PgState *s, const PgMove *m, int n); /* pg_init, then the moves; returns how many were legal (n = all) */

/* ---- graph games ---- */
int pg_nnodes(int g);
const char *pg_node_name(int g, int i);
int pg_lattice(int g);                             /* positions are 0..pg_lattice(g) (4, 8 or 1000) */
void pg_node_xy(int g, int i, int *x, int *y);
int pg_nlines(int g);
int pg_line(int g, int l, const unsigned char **nodes); /* the nodes of line l in order; returns their number */
int pg_graph_dests(int g, const PgState *s, int from, unsigned char dest[PG_MAXN]); /* dest[i] = 1 if the token on `from` may go to node i; returns how many */

/* ---- tile games ---- */
int pg_ntiles(int g);
int pg_nslots(int g);
int pg_side(int g);                                /* slots per row: 3, 5 or 4 */
int pg_tile_slot(int g, const PgState *s, int tile);   /* slot holding the tile, -1 if it is in the tray */
int pg_placed(int g, const PgState *s);
char pg_three_cell(int tile, int rot, int r, int c);   /* three of each: 'R' red, 'W' white, 'B' black or '.' */
int pg_pat_mask(int g, int tile, int rot, int k);      /* patterns: line ends of cell k (0 TL, 1 TR, 2 BL, 3 BR) as PG_N|PG_E|PG_S|PG_W */
enum { PG_N = 1, PG_E = 2, PG_S = 4, PG_W = 8 };
/* 0 solved, 1 not all tiles laid, 2 patterns: a line end meets an empty edge or leaves the square, 3 patterns: the line
 * closes into several loops, 4 three of each: some row, column or diagonal is wrong */
int pg_tile_check(int g, const PgState *s);

/* ---- swap blocks ---- */
int pg_swap_shape(int block);                      /* 0..3 */
int pg_swap_colour(int block);                     /* 0 plain, 1 red, 2 black */
int pg_swap_cells(int block, const PgState *s, int cells[8]);  /* the 8 cells (row * 8 + col) the block covers */
int pg_swap_block_at(const PgState *s, int cell);  /* block covering the cell, -1 if free */
int pg_swap_anchor(int block, const PgState *s);   /* the block's first cell in reading order (where its dots go) */
int pg_swap_origin_for_anchor(int block, int anchor_cell); /* the origin whose anchor is that cell, -1 if outside the box */
int pg_swap_dests(const PgState *s, int block, unsigned char dest[PG_SW_CELLS]); /* dest[origin] = 1; returns how many */

#endif
