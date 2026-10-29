/* Puzzle games: rules of the seven games (see pg_core.h). A port of the engines in il721/puzzles,
 * packages/module_playground; the boards and tiles are the source's tables, copied as they are. */
#include <string.h>

#include "pg_core.h"

static const char *const GAME_ID[PG_GAMES] = {"eight_chips", "cats_dogs", "hourglass", "three_each", "patterns5", "patterns4", "swap_blocks"};
static const int GAME_KIND[PG_GAMES] = {PGK_GRAPH, PGK_GRAPH, PGK_GRAPH, PGK_TILES, PGK_TILES, PGK_TILES, PGK_BLOCKS};
/* the minimum an exhaustive search proved (the source's module06.json); 0 = no move economy */
static const int GAME_PAR[PG_GAMES] = {28, 32, 26, 0, 0, 0, 32};

unsigned pg_pack(PgMove m) { return ((unsigned)m.k << 24) | ((unsigned)m.a << 16) | ((unsigned)m.b << 8) | m.c; }

PgMove pg_unpack(unsigned v) {
  PgMove m;
  m.k = (unsigned char)(v >> 24);
  m.a = (unsigned char)(v >> 16);
  m.b = (unsigned char)(v >> 8);
  m.c = (unsigned char)v;
  return m;
}

int pg_kind(int g) { return g >= 0 && g < PG_GAMES ? GAME_KIND[g] : -1; }
const char *pg_id(int g) { return g >= 0 && g < PG_GAMES ? GAME_ID[g] : ""; }
int pg_par(int g) { return g >= 0 && g < PG_GAMES ? GAME_PAR[g] : 0; }

/* ======================= graph games ======================= */

/* node names and lines exactly as in the source (token_graph boards); the tables below are built from them once */
static const char EIGHT_NODES[] = "P1 P2 P3 P4 P5 P6 P7 P8 C";
static const char *const EIGHT_LINES[] = {"P1 C P5", "P3 C P7", "P1 P4", "P1 P6", "P2 P5", "P2 P7", "P3 P6", "P3 P8", "P4 P7", "P5 P8"};
static const char CATS_NODES[] = "L1 L2 L3 R1 R2 R3 TM BM U1 U2 M D1 D2";
static const char *const CATS_LINES[] = {"L1 U1", "TM U1", "TM U2", "R1 U2", "U1 L2", "U1 M", "U2 M", "U2 R2",
                                         "L2 D1", "M D1", "M D2", "R2 D2", "D1 L3", "D1 BM", "D2 BM", "D2 R3"};
static const char HOUR_NODES[] =
    "A1 A2 A3 A4 A5 B1 B2 B3 B4 C1 C2 C3 D1 D2 E1 F1 F2 G1 G2 G3 H1 H2 H3 H4 I1 I2 I3 I4 I5";
static const char *const HOUR_LINES[] = {
    "A1 B1 C1 D1 E1 F2 G3 H4 I5", "A2 B2 C2 D2", "A3 B3 C3", "A4 B4", "F1 G2 H3 I4", "G1 H2 I3", "H1 I2",
    "A2 B1", "A3 B2 C1", "A4 B3 C2 D1", "A5 B4 C3 D2 E1 F1 G1 H1 I1", "F2 G2 H2 I2", "G3 H3 I3", "H4 I4"};

/* lattice positions (column, row) of the cats-and-dogs and hourglass nodes, in node order (source: *_board.dart) */
static const signed char CATS_XY[13][2] = {{0, 0}, {0, 2}, {0, 4}, {4, 0}, {4, 2}, {4, 4}, {2, 0}, {2, 4}, {1, 1}, {3, 1}, {2, 2}, {1, 3}, {3, 3}};
static const signed char HOUR_XY[29][2] = {{0, 0}, {2, 0}, {4, 0}, {6, 0}, {8, 0}, {1, 1}, {3, 1}, {5, 1}, {7, 1}, {2, 2}, {4, 2}, {6, 2}, {3, 3}, {5, 3}, {4, 4},
                                           {3, 5}, {5, 5}, {2, 6}, {4, 6}, {6, 6}, {1, 7}, {3, 7}, {5, 7}, {7, 7}, {0, 8}, {2, 8}, {4, 8}, {6, 8}, {8, 8}};

#define MAXLINES 16
#define MAXLEN 9
#define MAXDEG 8
typedef struct {
  int n, nl;
  char name[PG_MAXN][3];
  unsigned char line[MAXLINES][MAXLEN], len[MAXLINES];
  int nstep[PG_MAXN];
  unsigned char step[PG_MAXN][MAXDEG];
  int njump[PG_MAXN];
  unsigned char jover[PG_MAXN][MAXDEG], jland[PG_MAXN][MAXDEG];
  short x[PG_MAXN], y[PG_MAXN];
  int lattice;
} Graph;

static Graph GR[3];
static int graphs_built;

static int node_index(const Graph *gr, const char *s) {
  int i;
  for (i = 0; i < gr->n; i++)
    if (gr->name[i][0] == s[0] && gr->name[i][1] == s[1]) return i;
  return -1;
}

static void build_graph(Graph *gr, const char *nodes, const char *const *lines, int nlines) {
  const char *p;
  int l, i;
  memset(gr, 0, sizeof *gr);
  for (p = nodes; *p;) {
    while (*p == ' ') p++;
    if (!*p) break;
    gr->name[gr->n][0] = p[0];
    gr->name[gr->n][1] = p[1] == ' ' || !p[1] ? 0 : p[1];
    gr->n++;
    p += gr->name[gr->n - 1][1] ? 2 : 1;
  }
  for (l = 0; l < nlines; l++) {
    char t[3];
    for (p = lines[l]; *p;) {
      int k;
      while (*p == ' ') p++;
      if (!*p) break;
      t[0] = p[0];
      t[1] = p[1] == ' ' || !p[1] ? 0 : p[1];
      t[2] = 0;
      p += t[1] ? 2 : 1;
      k = node_index(gr, t);
      gr->line[l][gr->len[l]++] = (unsigned char)(k < 0 ? 0 : k);
    }
    for (i = 0; i + 1 < gr->len[l]; i++) {
      int a = gr->line[l][i], b = gr->line[l][i + 1];
      gr->step[a][gr->nstep[a]++] = (unsigned char)b;
      gr->step[b][gr->nstep[b]++] = (unsigned char)a;
    }
    for (i = 0; i + 2 < gr->len[l]; i++) {
      int a = gr->line[l][i], b = gr->line[l][i + 1], c = gr->line[l][i + 2];
      gr->jover[a][gr->njump[a]] = (unsigned char)b;
      gr->jland[a][gr->njump[a]++] = (unsigned char)c;
      gr->jover[c][gr->njump[c]] = (unsigned char)b;
      gr->jland[c][gr->njump[c]++] = (unsigned char)a;
    }
  }
  gr->nl = nlines;
}

static void build_graphs(void) {
  static const short SIN8[8] = {0, 707, 1000, 707, 0, -707, -1000, -707};
  Graph *g;
  int i;
  if (graphs_built) return;
  graphs_built = 1;
  g = &GR[PG_EIGHT];
  build_graph(g, EIGHT_NODES, EIGHT_LINES, 10);
  g->lattice = 1000;
  for (i = 0; i < 8; i++) { /* the tips P1..P8 run counter-clockwise from the top; the centre is the last node */
    int k = (8 - i) % 8;
    g->x[i] = (short)(500 + SIN8[k] / 2);
    g->y[i] = (short)(500 - SIN8[(k + 2) % 8] / 2);
  }
  g->x[8] = g->y[8] = 500;
  g = &GR[PG_CATS];
  build_graph(g, CATS_NODES, CATS_LINES, 16);
  g->lattice = 4;
  for (i = 0; i < 13; i++) { g->x[i] = CATS_XY[i][0]; g->y[i] = CATS_XY[i][1]; }
  g = &GR[PG_HOUR];
  build_graph(g, HOUR_NODES, HOUR_LINES, 14);
  g->lattice = 8;
  for (i = 0; i < 29; i++) { g->x[i] = HOUR_XY[i][0]; g->y[i] = HOUR_XY[i][1]; }
}

static Graph *graph_of(int g) {
  if (g < 0 || g > PG_HOUR) return NULL;
  build_graphs();
  return &GR[g];
}

int pg_nnodes(int g) { Graph *gr = graph_of(g); return gr ? gr->n : 0; }
const char *pg_node_name(int g, int i) { Graph *gr = graph_of(g); return gr && i >= 0 && i < gr->n ? gr->name[i] : ""; }
int pg_lattice(int g) { Graph *gr = graph_of(g); return gr ? gr->lattice : 0; }
int pg_nlines(int g) { Graph *gr = graph_of(g); return gr ? gr->nl : 0; }

void pg_node_xy(int g, int i, int *x, int *y) {
  Graph *gr = graph_of(g);
  *x = *y = 0;
  if (gr && i >= 0 && i < gr->n) { *x = gr->x[i]; *y = gr->y[i]; }
}

int pg_line(int g, int l, const unsigned char **nodes) {
  Graph *gr = graph_of(g);
  if (!gr || l < 0 || l >= gr->nl) return 0;
  *nodes = gr->line[l];
  return gr->len[l];
}

/* cats and dogs: no cat may stand next to a dog (every line has two nodes, so "next to" = "on one line") */
static int peaceful(const Graph *gr, const signed char *tok) {
  int l;
  for (l = 0; l < gr->nl; l++) {
    int a = tok[gr->line[l][0]], b = tok[gr->line[l][1]];
    if (a && b && a != b) return 0;
  }
  return 1;
}

int pg_graph_dests(int g, const PgState *s, int from, unsigned char dest[PG_MAXN]) {
  Graph *gr = graph_of(g);
  int n = 0, i, l, k;
  memset(dest, 0, PG_MAXN);
  if (!gr || from < 0 || from >= gr->n || !s->tok[from]) return 0;
  if (g == PG_HOUR) {
    /* a step to a free neighbour, or a cascade of jumps over occupied neighbours onto free nodes (any landing is a destination) */
    unsigned char seen[PG_MAXN], queue[PG_MAXN];
    int qh = 0, qt = 0;
    memset(seen, 0, sizeof seen);
    for (k = 0; k < gr->nstep[from]; k++)
      if (!s->tok[gr->step[from][k]]) dest[gr->step[from][k]] = 1;
    seen[from] = 1;
    queue[qt++] = (unsigned char)from;
    while (qh < qt) {
      int cur = queue[qh++];
      for (k = 0; k < gr->njump[cur]; k++) {
        int over = gr->jover[cur][k], land = gr->jland[cur][k];
        if (over == from || !s->tok[over] || s->tok[land] || seen[land]) continue;
        seen[land] = 1;
        dest[land] = 1;
        queue[qt++] = (unsigned char)land;
      }
    }
  } else {
    /* slide along a line over free nodes until the first occupied one */
    for (l = 0; l < gr->nl; l++) {
      int len = gr->len[l], pos = -1, dir;
      for (i = 0; i < len; i++)
        if (gr->line[l][i] == from) pos = i;
      if (pos < 0) continue;
      for (dir = -1; dir <= 1; dir += 2)
        for (i = pos + dir; i >= 0 && i < len && !s->tok[gr->line[l][i]]; i += dir) dest[gr->line[l][i]] = 1;
    }
    if (g == PG_CATS) {
      for (i = 0; i < gr->n; i++) {
        signed char t[PG_MAXN];
        if (!dest[i]) continue;
        memcpy(t, s->tok, sizeof t);
        t[i] = t[from];
        t[from] = 0;
        if (!peaceful(gr, t)) dest[i] = 0;
      }
    }
  }
  for (i = 0; i < gr->n; i++) n += dest[i];
  return n;
}

static int graph_solved(int g, const PgState *s) {
  int i;
  if (g == PG_EIGHT) { /* chip t on tip P(9-t), the centre free */
    if (s->tok[8]) return 0;
    for (i = 0; i < 8; i++)
      if (s->tok[i] != 8 - i) return 0;
    return 1;
  }
  if (g == PG_CATS) { /* the cats (1) on the right seats, the dogs (2) on the left ones */
    for (i = 0; i < 3; i++)
      if (s->tok[i] != 2 || s->tok[3 + i] != 1) return 0;
    return 1;
  }
  for (i = 14; i < 29; i++) /* hourglass: the lower triangle (E1 and below) full */
    if (!s->tok[i]) return 0;
  return 1;
}

static void graph_init(int g, PgState *s) {
  int i;
  if (g == PG_EIGHT) for (i = 0; i < 8; i++) s->tok[i] = (signed char)(i + 1);
  else if (g == PG_CATS) for (i = 0; i < 3; i++) { s->tok[i] = 1; s->tok[3 + i] = 2; }
  else for (i = 0; i < 15; i++) s->tok[i] = 1; /* the upper triangle A1..E1 */
}

/* ======================= tile games ======================= */

/* "Three of each": the nine 3 x 3 tiles, unrotated (R red, W white, B black) */
static const char *const THREE_TILES[9][3] = {
    {"..R", "RB.", "..W"}, {"W..", "...", ".B."}, {"...", "R.W", ".B."}, {"B..", ".WR", ".R."}, {".B.", "W..", "B.R"},
    {"W..", "...", "..."}, {"R.B", "B..", ".W."}, {"W..", "..R", "..."}, {"B..", "W.R", "..."}};

/* "Patterns": a tile is 2 x 2 cells (top-left, top-right, bottom-left, bottom-right), each a connector joining two sides */
static const char *const PAT5_TILES[25][4] = {
    {"NE", "SW", "SW", "NE"}, {"NE", "NW", "SW", "SE"}, {"EW", "NW", "SW", "SE"}, {"NE", "EW", "SW", "SE"}, {"EW", "EW", "SW", "SE"},
    {"EW", "SW", "SW", "NS"}, {"NE", "SW", "SW", "NS"}, {"EW", "SW", "SW", "NE"}, {"NE", "SW", "SW", "NE"}, {"SE", "SW", "NW", "NE"},
    {"SE", "SW", "NW", "NS"}, {"SE", "SW", "NS", "NE"}, {"SE", "SW", "NS", "NS"}, {"NE", "NW", "SE", "SW"}, {"NE", "NW", "SE", "EW"},
    {"NE", "NW", "EW", "SW"}, {"NE", "NW", "EW", "EW"}, {"EW", "EW", "EW", "EW"}, {"EW", "EW", "SE", "EW"}, {"EW", "EW", "EW", "SW"},
    {"NE", "EW", "SE", "EW"}, {"NE", "EW", "EW", "SW"}, {"EW", "NW", "SE", "EW"}, {"NS", "SE", "NE", "NW"}, {"SW", "NS", "NE", "NW"}};
static const char *const PAT4_TILES[16][4] = {
    {"NE", "NW", "SW", "SE"}, {"EW", "EW", "SW", "SE"}, {"EW", "SW", "SW", "NS"}, {"EW", "SW", "SW", "NE"}, {"SE", "SW", "NW", "NE"},
    {"SE", "SW", "NW", "NS"}, {"SE", "SW", "NS", "NE"}, {"SE", "SW", "NS", "NS"}, {"NE", "NW", "SE", "SW"}, {"NE", "NW", "SE", "EW"},
    {"EW", "EW", "EW", "EW"}, {"EW", "EW", "SE", "EW"}, {"EW", "EW", "EW", "SW"}, {"NE", "EW", "EW", "SW"}, {"NS", "SE", "NE", "NW"},
    {"SW", "NS", "NE", "NW"}};

int pg_ntiles(int g) { return g == PG_THREE ? 9 : g == PG_PAT5 ? 25 : g == PG_PAT4 ? 16 : 0; }
int pg_nslots(int g) { return g == PG_THREE ? 9 : g == PG_PAT5 ? 25 : g == PG_PAT4 ? 16 : 0; }
int pg_side(int g) { return g == PG_THREE ? 3 : g == PG_PAT5 ? 5 : g == PG_PAT4 ? 4 : 0; }

int pg_tile_slot(int g, const PgState *s, int tile) {
  int i, n = pg_nslots(g);
  for (i = 0; i < n; i++)
    if (s->tile[i] == tile) return i;
  return -1;
}

int pg_placed(int g, const PgState *s) {
  int i, n = pg_nslots(g), c = 0;
  for (i = 0; i < n; i++) c += s->tile[i] >= 0;
  return c;
}

char pg_three_cell(int tile, int rot, int r, int c) {
  int row = r, col = c, i;
  if (tile < 0 || tile > 8) return '.';
  for (i = 0; i < (rot & 3); i++) { /* one clockwise turn: the cell shown at (r, c) is (2 - c, r) of the turn before */
    int nr = 2 - col, nc = row;
    row = nr;
    col = nc;
  }
  return THREE_TILES[tile][row][col];
}

static int conn_mask(const char *c) {
  int m = 0;
  for (; *c; c++) m |= *c == 'N' ? PG_N : *c == 'E' ? PG_E : *c == 'S' ? PG_S : PG_W;
  return m;
}

static int rot_mask(int m) {
  int o = 0;
  if (m & PG_N) o |= PG_E;
  if (m & PG_E) o |= PG_S;
  if (m & PG_S) o |= PG_W;
  if (m & PG_W) o |= PG_N;
  return o;
}

int pg_pat_mask(int g, int tile, int rot, int k) {
  const char *const *t;
  int m[4], i, j;
  if (g != PG_PAT5 && g != PG_PAT4) return 0;
  if (tile < 0 || tile >= pg_ntiles(g) || k < 0 || k > 3) return 0;
  t = g == PG_PAT5 ? PAT5_TILES[tile] : PAT4_TILES[tile];
  for (i = 0; i < 4; i++) m[i] = conn_mask(t[i]);
  for (j = 0; j < (rot & 3); j++) { /* a clockwise turn moves BL -> TL -> TR -> BR -> BL and turns each connector with it */
    int n0 = rot_mask(m[2]), n1 = rot_mask(m[0]), n2 = rot_mask(m[3]), n3 = rot_mask(m[1]);
    m[0] = n0; m[1] = n1; m[2] = n2; m[3] = n3;
  }
  return m[k];
}

static int three_check(const PgState *s) {
  char grid[9][9];
  int sl, r, c, l, i;
  for (sl = 0; sl < 9; sl++) {
    if (s->tile[sl] < 0) return 1;
    for (r = 0; r < 3; r++)
      for (c = 0; c < 3; c++) grid[(sl / 3) * 3 + r][(sl % 3) * 3 + c] = pg_three_cell(s->tile[sl], s->rot[sl], r, c);
  }
  for (l = 0; l < 20; l++) { /* nine rows, nine columns, two diagonals: one red, one white and one black circle on each */
    int cnt[3] = {0, 0, 0};
    for (i = 0; i < 9; i++) {
      int rr = l < 9 ? l : i, cc = l < 9 ? i : l < 18 ? l - 9 : l == 18 ? i : 8 - i;
      char ch = grid[rr][cc];
      if (ch == 'R') cnt[0]++;
      else if (ch == 'W') cnt[1]++;
      else if (ch == 'B') cnt[2]++;
    }
    if (cnt[0] != 1 || cnt[1] != 1 || cnt[2] != 1) return 4;
  }
  return 0;
}

static int pat_check(int g, const PgState *s) {
  int side = pg_side(g), cs = side * 2, total = cs * cs, sl, k, r, c;
  int cells[100];
  int cell, entered, steps;
  memset(cells, 0, sizeof cells);
  for (sl = 0; sl < side * side; sl++) {
    if (s->tile[sl] < 0) return 1;
    for (k = 0; k < 4; k++) cells[((sl / side) * 2 + k / 2) * cs + (sl % side) * 2 + k % 2] = pg_pat_mask(g, s->tile[sl], s->rot[sl], k);
  }
  for (r = 0; r < cs; r++)
    for (c = 0; c < cs; c++) {
      int m = cells[r * cs + c];
      if ((m & PG_N) && (r == 0 || !(cells[(r - 1) * cs + c] & PG_S))) return 2;
      if ((m & PG_S) && (r == cs - 1 || !(cells[(r + 1) * cs + c] & PG_N))) return 2;
      if ((m & PG_W) && (c == 0 || !(cells[r * cs + c - 1] & PG_E))) return 2;
      if ((m & PG_E) && (c == cs - 1 || !(cells[r * cs + c + 1] & PG_W))) return 2;
    }
  /* every cell has exactly two ends, so a matched board is a union of loops: one walk from cell 0 that returns to it after
   * visiting every cell means one loop */
  cell = 0; entered = 0; steps = 0;
  do {
    int ex = cells[cell] & ~entered, dir;
    if (!ex) return 3;
    dir = ex & -ex;
    r = cell / cs;
    c = cell % cs;
    if (dir == PG_N) { cell = (r - 1) * cs + c; entered = PG_S; }
    else if (dir == PG_S) { cell = (r + 1) * cs + c; entered = PG_N; }
    else if (dir == PG_W) { cell = r * cs + c - 1; entered = PG_E; }
    else { cell = r * cs + c + 1; entered = PG_W; }
    steps++;
  } while (cell != 0 && steps <= total);
  return steps == total ? 0 : 3;
}

int pg_tile_check(int g, const PgState *s) {
  if (g == PG_THREE) return three_check(s);
  if (g == PG_PAT5 || g == PG_PAT4) return pat_check(g, s);
  return 1;
}

/* ======================= swap blocks ======================= */

/* the four P-shapes: 8 cells (row, col) in a 3 x 4 box; the anchor is the first cell in reading order */
static const signed char SHAPE[4][8][2] = {
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1}, {2, 0}, {2, 1}},
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 2}, {2, 3}},
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}, {2, 1}, {2, 2}, {2, 3}},
    {{0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 0}, {2, 1}, {2, 2}, {2, 3}}};
static const signed char ANCHOR[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 2}};
static const unsigned char BLOCK_SHAPE[PG_BLOCKS] = {0, 1, 1, 2, 2, 3, 0, 1, 1, 2, 2, 3};
static const unsigned char BLOCK_COLOUR[PG_BLOCKS] = {0, 0, 1, 1, 0, 0, 0, 0, 2, 2, 0, 0};
static const unsigned char START_ORG[PG_BLOCKS] = {0, 4, 10, 18, 24, 28, 80, 84, 90, 98, 104, 108};

int pg_swap_shape(int block) { return block >= 0 && block < PG_BLOCKS ? BLOCK_SHAPE[block] : 0; }
int pg_swap_colour(int block) { return block >= 0 && block < PG_BLOCKS ? BLOCK_COLOUR[block] : 0; }

int pg_swap_cells(int block, const PgState *s, int cells[8]) {
  int r0 = s->org[block] / PG_SW_COLS, c0 = s->org[block] % PG_SW_COLS, k;
  for (k = 0; k < 8; k++) cells[k] = (r0 + SHAPE[BLOCK_SHAPE[block]][k][0]) * PG_SW_COLS + c0 + SHAPE[BLOCK_SHAPE[block]][k][1];
  return 8;
}

int pg_swap_block_at(const PgState *s, int cell) {
  int b, k;
  for (b = 0; b < PG_BLOCKS; b++) {
    int cells[8];
    pg_swap_cells(b, s, cells);
    for (k = 0; k < 8; k++)
      if (cells[k] == cell) return b;
  }
  return -1;
}

int pg_swap_anchor(int block, const PgState *s) {
  int sh = BLOCK_SHAPE[block];
  return (s->org[block] / PG_SW_COLS + ANCHOR[sh][0]) * PG_SW_COLS + s->org[block] % PG_SW_COLS + ANCHOR[sh][1];
}

int pg_swap_origin_for_anchor(int block, int anchor_cell) {
  int sh = BLOCK_SHAPE[block], r, c;
  if (anchor_cell < 0 || anchor_cell >= PG_SW_CELLS) return -1;
  r = anchor_cell / PG_SW_COLS - ANCHOR[sh][0];
  c = anchor_cell % PG_SW_COLS - ANCHOR[sh][1];
  if (r < 0 || c < 0 || r + 2 >= PG_SW_ROWS || c + 3 >= PG_SW_COLS) return -1;
  return r * PG_SW_COLS + c;
}

int pg_swap_dests(const PgState *s, int block, unsigned char dest[PG_SW_CELLS]) {
  unsigned char other[PG_SW_CELLS], seen[PG_SW_CELLS];
  int queue[PG_SW_CELLS], qh = 0, qt = 0, b, k, n = 0, sh;
  memset(dest, 0, PG_SW_CELLS);
  if (block < 0 || block >= PG_BLOCKS) return 0;
  memset(other, 0, sizeof other);
  memset(seen, 0, sizeof seen);
  for (b = 0; b < PG_BLOCKS; b++) {
    int cells[8];
    if (b == block) continue;
    pg_swap_cells(b, s, cells);
    for (k = 0; k < 8; k++) other[cells[k]] = 1;
  }
  sh = BLOCK_SHAPE[block];
  seen[s->org[block]] = 1;
  queue[qt++] = s->org[block];
  while (qh < qt) { /* the block slides orthogonally through free space, corners allowed: flood over its origins */
    int cur = queue[qh++], r = cur / PG_SW_COLS, c = cur % PG_SW_COLS, d;
    static const signed char DR[4] = {-1, 1, 0, 0}, DC[4] = {0, 0, -1, 1};
    for (d = 0; d < 4; d++) {
      int nr = r + DR[d], nc = c + DC[d], ok = 1;
      if (nr < 0 || nc < 0 || nr + 2 >= PG_SW_ROWS || nc + 3 >= PG_SW_COLS) continue;
      if (seen[nr * PG_SW_COLS + nc]) continue;
      for (k = 0; k < 8 && ok; k++)
        if (other[(nr + SHAPE[sh][k][0]) * PG_SW_COLS + nc + SHAPE[sh][k][1]]) ok = 0;
      if (!ok) continue;
      seen[nr * PG_SW_COLS + nc] = 1;
      dest[nr * PG_SW_COLS + nc] = 1;
      n++;
      queue[qt++] = nr * PG_SW_COLS + nc;
    }
  }
  return n;
}

/* solved: the start layout with the red pair and the black pair (blocks 2/8) and blocks 3/9 exchanged, blocks of the same
 * shape and colour being interchangeable - compare the two layouts cell by cell, painted with each block's class */
static void paint(const unsigned char *org, unsigned char board[PG_SW_CELLS]) {
  int b, k;
  memset(board, 0, PG_SW_CELLS);
  for (b = 0; b < PG_BLOCKS; b++)
    for (k = 0; k < 8; k++)
      board[(org[b] / PG_SW_COLS + SHAPE[BLOCK_SHAPE[b]][k][0]) * PG_SW_COLS + org[b] % PG_SW_COLS + SHAPE[BLOCK_SHAPE[b]][k][1]] =
          (unsigned char)(1 + BLOCK_SHAPE[b] * 3 + BLOCK_COLOUR[b]);
}

static int swap_solved(const PgState *s) {
  unsigned char goal[PG_BLOCKS], a[PG_SW_CELLS], b[PG_SW_CELLS];
  memcpy(goal, START_ORG, sizeof goal);
  goal[2] = START_ORG[8];
  goal[8] = START_ORG[2];
  goal[3] = START_ORG[9];
  goal[9] = START_ORG[3];
  paint(s->org, a);
  paint(goal, b);
  return memcmp(a, b, sizeof a) == 0;
}

/* ======================= common ======================= */

void pg_init(int g, PgState *s) {
  memset(s, 0, sizeof *s);
  memset(s->tile, -1, sizeof s->tile);
  if (g >= 0 && g <= PG_HOUR) graph_init(g, s);
  else memcpy(s->org, START_ORG, sizeof s->org);
}

int pg_solved(int g, const PgState *s) {
  if (g >= 0 && g <= PG_HOUR) return graph_solved(g, s);
  if (g == PG_SWAP) return swap_solved(s);
  return pg_tile_check(g, s) == 0;
}

int pg_apply(int g, PgState *s, PgMove m) {
  int kind = pg_kind(g);
  if (kind == PGK_GRAPH) {
    unsigned char dest[PG_MAXN];
    if (m.k != PGM_SLIDE || m.a >= pg_nnodes(g) || m.b >= pg_nnodes(g)) return 0;
    pg_graph_dests(g, s, m.a, dest);
    if (!dest[m.b]) return 0;
    s->tok[m.b] = s->tok[m.a];
    s->tok[m.a] = 0;
    return 1;
  }
  if (kind == PGK_TILES) {
    int nt = pg_ntiles(g), ns = pg_nslots(g);
    if (m.k == PGM_PLACE) {
      if (m.a >= nt || m.b >= ns || m.c > 3 || pg_tile_slot(g, s, m.a) >= 0 || s->tile[m.b] >= 0) return 0;
      s->tile[m.b] = (signed char)m.a;
      s->rot[m.b] = (signed char)m.c;
      return 1;
    }
    if (m.k == PGM_TURN) {
      if (m.a >= ns || m.b > 3 || s->tile[m.a] < 0 || s->rot[m.a] == m.b) return 0;
      s->rot[m.a] = (signed char)m.b;
      return 1;
    }
    if (m.k == PGM_TAKE) {
      if (m.a >= ns || s->tile[m.a] < 0) return 0;
      s->tile[m.a] = -1;
      s->rot[m.a] = 0;
      return 1;
    }
    return 0;
  }
  if (kind == PGK_BLOCKS) {
    unsigned char dest[PG_SW_CELLS];
    if (m.k != PGM_BLOCK || m.a >= PG_BLOCKS || m.b >= PG_SW_CELLS) return 0;
    pg_swap_dests(s, m.a, dest);
    if (!dest[m.b]) return 0;
    s->org[m.a] = m.b;
    return 1;
  }
  return 0;
}

int pg_replay(int g, PgState *s, const PgMove *m, int n) {
  int i;
  pg_init(g, s);
  for (i = 0; i < n; i++)
    if (!pg_apply(g, s, m[i])) return i;
  return n;
}
