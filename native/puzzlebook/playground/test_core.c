/* Host test for pg_core: gcc -O2 -o /tmp/t test_core.c pg_core.c && /tmp/t
 * The source's recorded solutions are replayed through the C engines, the proven minimums of eight chips and cats and dogs
 * are re-found by breadth-first search, and the number of solutions of the tile games is counted (16, 512, 2048 - the
 * numbers the source's own search found). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pg_core.h"
#include "playground_solutions.h"

static int fails, checks;
#define PHASE(f) do { clock_t t0 = clock(); f; printf("  %-18s %5.1fs\n", #f, (double)(clock() - t0) / CLOCKS_PER_SEC); fflush(stdout); } while (0)

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static int node_of(int g, const char *s, int n) {
  int i;
  for (i = 0; i < pg_nnodes(g); i++)
    if (strlen(pg_node_name(g, i)) == (size_t)n && !strncmp(pg_node_name(g, i), s, (size_t)n)) return i;
  return -1;
}

/* one id of a move path: the text from s up to the next space or the end */
static const char *next_id(const char *s, int *n) {
  const char *e = s;
  while (*e && *e != ' ' && *e != ';') e++;
  *n = (int)(e - s);
  return e;
}

/* "T4 S6r2" (place), "S6 S6r3" (turn), "S6 T4" (take), "P1 C", "r4c7 r5c7" -> a PgMove; the path may have more ids (hourglass) */
static PgMove parse_move(int g, const char *m, const PgState *s, int *ok) {
  PgMove mv = {0, 0, 0, 0};
  const char *ids[12];
  int len[12], n = 0;
  const char *p = m;
  *ok = 0;
  while (n < 12) {
    ids[n] = p;
    p = next_id(p, &len[n]);
    n++;
    if (*p != ' ') break;
    p++;
  }
  if (n < 2) return mv;
  if (pg_kind(g) == PGK_GRAPH) {
    int a = node_of(g, ids[0], len[0]), b = node_of(g, ids[n - 1], len[n - 1]);
    if (a < 0 || b < 0) return mv;
    mv.k = PGM_SLIDE; mv.a = (unsigned char)a; mv.b = (unsigned char)b;
    *ok = 1;
  } else if (pg_kind(g) == PGK_TILES) {
    int fa = atoi(ids[0] + 1) - 1, to_slot = atoi(ids[1] + 1) - 1;
    if (ids[0][0] == 'T') { /* tile -> slot at a rotation */
      mv.k = PGM_PLACE; mv.a = (unsigned char)fa; mv.b = (unsigned char)to_slot; mv.c = (unsigned char)(ids[1][len[1] - 1] - '0');
      *ok = 1;
    } else if (ids[1][0] == 'T') { /* slot -> tile: back to the tray */
      mv.k = PGM_TAKE; mv.a = (unsigned char)fa;
      *ok = 1;
    } else { /* slot -> slot with a new rotation */
      mv.k = PGM_TURN; mv.a = (unsigned char)fa; mv.b = (unsigned char)(ids[1][len[1] - 1] - '0');
      *ok = 1;
    }
  } else {
    int fr = atoi(ids[0] + 1) - 1, fc = atoi(strchr(ids[0], 'c') + 1) - 1, tr_ = atoi(ids[1] + 1) - 1, tc = atoi(strchr(ids[1], 'c') + 1) - 1;
    int b = pg_swap_block_at(s, fr * PG_SW_COLS + fc), k;
    for (k = 0; k < PG_BLOCKS && b >= 0; k++)
      if (pg_swap_anchor(k, s) == fr * PG_SW_COLS + fc) { b = k; break; }
    if (b < 0) return mv;
    mv.k = PGM_BLOCK; mv.a = (unsigned char)b;
    mv.b = (unsigned char)pg_swap_origin_for_anchor(b, tr_ * PG_SW_COLS + tc);
    *ok = mv.b < PG_SW_CELLS;
  }
  return mv;
}

static void test_solutions(void) {
  int g, i;
  CHECK(PG_NUM_SOLUTIONS == PG_GAMES);
  for (g = 0; g < PG_GAMES; g++) {
    const PgSolution *sol = &PG_SOLUTIONS[g];
    PgState s;
    const char *p = sol->moves;
    int n = 0, before_last_solved = 0;
    CHECK(!strcmp(sol->id, pg_id(g)));
    CHECK(sol->par == pg_par(g));
    pg_init(g, &s);
    CHECK(!pg_solved(g, &s));
    while (*p) {
      int ok;
      PgMove mv = parse_move(g, p, &s, &ok);
      CHECK(ok);
      before_last_solved = pg_solved(g, &s);
      if (!ok || !pg_apply(g, &s, mv)) { CHECK(0 && "recorded move is legal"); printf("  %s move %d\n", sol->id, n + 1); break; }
      n++;
      while (*p && *p != ';') p++;
      if (*p == ';') p++;
    }
    CHECK(pg_solved(g, &s));
    CHECK(!before_last_solved);
    if (sol->par) CHECK(n == sol->par);
    (void)i;
  }
}

/* ---- breadth-first search over the graph games ---- */

static unsigned long long key_of(int g, const PgState *s) {
  int n = pg_nnodes(g), i;
  unsigned long long k = 0;
  for (i = 0; i < n; i++) k = k * 9 + (unsigned long long)s->tok[i];
  return k;
}

static int bfs_par(int g) {
  /* eight chips: 9^9 keys as a bitset; cats and dogs: 3^13 keys */
  unsigned long long space = g == PG_EIGHT ? 387420489ULL : 1594323ULL; /* 9^9 or 3^13 keys */
  unsigned char *seen = calloc((size_t)(space / 8 + 1), 1);
  size_t cap = 1 << 20, head = 0, tail = 0;
  PgState *q = malloc(cap * sizeof(PgState));
  int *dist = malloc(cap * sizeof(int)), ans = -1;
  PgState s;
  pg_init(g, &s);
  q[tail] = s; dist[tail++] = 0;
  {
    unsigned long long k = g == PG_EIGHT ? key_of(g, &s) : 0;
    if (g == PG_CATS) { int i; for (i = 0; i < 13; i++) k = k * 3 + (unsigned long long)s.tok[i]; }
    seen[k / 8] |= (unsigned char)(1 << (k % 8));
  }
  while (head < tail && ans < 0) {
    PgState cur = q[head];
    int d = dist[head++], from, to;
    for (from = 0; from < pg_nnodes(g) && ans < 0; from++) {
      unsigned char dest[PG_MAXN];
      if (!cur.tok[from]) continue;
      pg_graph_dests(g, &cur, from, dest);
      for (to = 0; to < pg_nnodes(g); to++) {
        PgState nx;
        unsigned long long k = 0;
        if (!dest[to]) continue;
        nx = cur;
        nx.tok[to] = nx.tok[from];
        nx.tok[from] = 0;
        if (g == PG_EIGHT) k = key_of(g, &nx);
        else { int i; for (i = 0; i < 13; i++) k = k * 3 + (unsigned long long)nx.tok[i]; }
        if (seen[k / 8] & (1 << (k % 8))) continue;
        seen[k / 8] |= (unsigned char)(1 << (k % 8));
        if (pg_solved(g, &nx)) { ans = d + 1; break; }
        if (tail == cap) { cap *= 2; q = realloc(q, cap * sizeof(PgState)); dist = realloc(dist, cap * sizeof(int)); }
        q[tail] = nx; dist[tail++] = d + 1;
      }
    }
  }
  free(seen); free(q); free(dist);
  return ans;
}

/* ---- hand-made rules ---- */

static int ndest(int g, const PgState *s, int from, const char *want) { /* the destinations of `from` are exactly the names in want (space separated) */
  unsigned char dest[PG_MAXN];
  int i, n = pg_graph_dests(g, s, from, dest), listed = 0;
  char buf[200];
  strncpy(buf, want, sizeof buf - 1);
  buf[sizeof buf - 1] = 0;
  if (buf[0]) {
    char *t;
    for (t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
      int k = node_of(g, t, (int)strlen(t));
      if (k < 0 || !dest[k]) return 0;
      listed++;
    }
  }
  for (i = 0; i < pg_nnodes(g); i++) if (dest[i] && node_of(g, pg_node_name(g, i), (int)strlen(pg_node_name(g, i))) != i) return 0;
  return n == listed;
}

static void test_graph_rules(void) {
  PgState s;
  int g, i;
  /* eight chips: only the centre is free at the start, so a chip on a line through it slides in; a chip on a spoke can
   * not slide through an occupied tip */
  pg_init(PG_EIGHT, &s);
  CHECK(pg_nnodes(PG_EIGHT) == 9 && pg_nlines(PG_EIGHT) == 10);
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P1", 2), "C"));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P5", 2), "C"));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P3", 2), "C"));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P2", 2), ""));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "C", 1), ""));       /* the centre holds no chip */
  /* P1 -> C: now P4 and P6 may step into P1's place, and so may the chip on C (P5 is occupied, so it can only go back) */
  { PgMove m = {PGM_SLIDE, 0, 8, 0}; CHECK(pg_apply(PG_EIGHT, &s, m)); }
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P4", 2), "P1"));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "P6", 2), "P1"));
  CHECK(ndest(PG_EIGHT, &s, node_of(PG_EIGHT, "C", 1), "P1"));     /* the chip on C slides back to P1 (P5 is occupied) */
  { PgMove m = {PGM_SLIDE, 3, 0, 0}; CHECK(pg_apply(PG_EIGHT, &s, m)); }
  { PgMove m = {PGM_SLIDE, 3, 0, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); } /* P4 is free now */
  { PgMove m = {PGM_SLIDE, 1, 0, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); } /* P2 -> P1: not on one line, and P1 is taken */
  { PgMove m = {PGM_SLIDE, 0, 8, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); } /* C is occupied */
  { PgMove m = {PGM_SLIDE, 3, 5, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); } /* from a free node */
  { PgMove m = {PGM_SLIDE, 40, 8, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); }
  { PgMove m = {PGM_PLACE, 0, 8, 0}; CHECK(!pg_apply(PG_EIGHT, &s, m)); }

  /* hourglass: the waist chip steps to F1 or F2, D1 and D2 jump over it; nothing else can move */
  pg_init(PG_HOUR, &s);
  CHECK(pg_nnodes(PG_HOUR) == 29 && pg_nlines(PG_HOUR) == 14);
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "E1", 2), "F1 F2"));
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "D1", 2), "F2"));
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "D2", 2), "F1"));
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "C1", 2), ""));
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "A1", 2), ""));
  /* a cascade, built by hand: a chip on A1 and chips on B1, D1, F2 and H4 -> A1 jumps B1 (lands C1), then D1 (E1), F2 (G3), H4 (I5);
   * every landing is a destination (a cascade may stop anywhere) */
  memset(s.tok, 0, sizeof s.tok);
  s.tok[node_of(PG_HOUR, "A1", 2)] = 1;
  s.tok[node_of(PG_HOUR, "B1", 2)] = 1;
  s.tok[node_of(PG_HOUR, "D1", 2)] = 1;
  s.tok[node_of(PG_HOUR, "F2", 2)] = 1;
  s.tok[node_of(PG_HOUR, "H4", 2)] = 1;
  CHECK(ndest(PG_HOUR, &s, node_of(PG_HOUR, "A1", 2), "C1 E1 G3 I5"));
  { PgMove m = {PGM_SLIDE, (unsigned char)node_of(PG_HOUR, "A1", 2), (unsigned char)node_of(PG_HOUR, "I5", 2), 0}; CHECK(pg_apply(PG_HOUR, &s, m)); }
  CHECK(s.tok[node_of(PG_HOUR, "I5", 2)] == 1 && s.tok[node_of(PG_HOUR, "A1", 2)] == 0 && s.tok[node_of(PG_HOUR, "B1", 2)] == 1); /* jumped chips stay */

  /* cats and dogs: a step that would put a cat next to a dog is never offered */
  pg_init(PG_CATS, &s);
  CHECK(pg_nnodes(PG_CATS) == 13 && pg_nlines(PG_CATS) == 16);
  CHECK(ndest(PG_CATS, &s, node_of(PG_CATS, "L1", 2), "U1"));
  CHECK(ndest(PG_CATS, &s, node_of(PG_CATS, "L2", 2), "U1 D1"));
  CHECK(ndest(PG_CATS, &s, node_of(PG_CATS, "R2", 2), "U2 D2"));
  { /* cat L1 -> U1: it may go back, to TM or to M; once a dog stands on U2 (R1 -> U2) TM and M are next to it and are no longer offered */
    PgMove m = {PGM_SLIDE, (unsigned char)node_of(PG_CATS, "L1", 2), (unsigned char)node_of(PG_CATS, "U1", 2), 0};
    PgMove d = {PGM_SLIDE, (unsigned char)node_of(PG_CATS, "R1", 2), (unsigned char)node_of(PG_CATS, "U2", 2), 0};
    CHECK(pg_apply(PG_CATS, &s, m));
    CHECK(ndest(PG_CATS, &s, node_of(PG_CATS, "U1", 2), "L1 TM M"));
    CHECK(pg_apply(PG_CATS, &s, d));
    CHECK(ndest(PG_CATS, &s, node_of(PG_CATS, "U1", 2), "L1"));
    { PgMove bad = {PGM_SLIDE, (unsigned char)node_of(PG_CATS, "U1", 2), (unsigned char)node_of(PG_CATS, "M", 1), 0}; CHECK(!pg_apply(PG_CATS, &s, bad)); }
  }
  /* random play keeps the pieces: the token counts never change and no cat stands next to a dog */
  for (g = PG_EIGHT; g <= PG_HOUR; g++) {
    int steps, counts0[9] = {0}, counts[9];
    pg_init(g, &s);
    for (i = 0; i < pg_nnodes(g); i++) counts0[(int)s.tok[i]]++;
    srand(7 + (unsigned)g);
    for (steps = 0; steps < 4000; steps++) {
      unsigned char dest[PG_MAXN];
      int from = rand() % pg_nnodes(g), n = pg_graph_dests(g, &s, from, dest), pick, to = 0;
      if (!n) continue;
      pick = rand() % n;
      for (to = 0; to < pg_nnodes(g); to++) if (dest[to] && pick-- == 0) break;
      { PgMove m = {PGM_SLIDE, (unsigned char)from, (unsigned char)to, 0}; CHECK(pg_apply(g, &s, m)); }
      if (g == PG_CATS) {
        int l;
        for (l = 0; l < pg_nlines(g); l++) {
          const unsigned char *nd;
          pg_line(g, l, &nd);
          CHECK(!(s.tok[nd[0]] && s.tok[nd[1]] && s.tok[nd[0]] != s.tok[nd[1]]));
        }
      }
    }
    memset(counts, 0, sizeof counts);
    for (i = 0; i < pg_nnodes(g); i++) counts[(int)s.tok[i]]++;
    CHECK(!memcmp(counts, counts0, sizeof counts));
  }
  /* lattice positions are inside the lattice and no two nodes share one */
  for (g = PG_EIGHT; g <= PG_HOUR; g++) {
    int a, b, x, y, x2, y2, L = pg_lattice(g);
    for (a = 0; a < pg_nnodes(g); a++) {
      pg_node_xy(g, a, &x, &y);
      CHECK(x >= 0 && x <= L && y >= 0 && y <= L);
      for (b = a + 1; b < pg_nnodes(g); b++) { pg_node_xy(g, b, &x2, &y2); CHECK(x != x2 || y != y2); }
    }
  }
}

/* ---- tile games ---- */

static int pat_count_solutions(int g);
static int three_count_solutions(void);

static void test_tile_rules(void) {
  PgState s;
  int t, r, c, k, g, cnt[3];
  /* three of each: nine circles of each colour on the nine tiles; a quarter turn moves the cell (r, c) to (c, 2 - r) */
  cnt[0] = cnt[1] = cnt[2] = 0;
  for (t = 0; t < 9; t++)
    for (r = 0; r < 3; r++)
      for (c = 0; c < 3; c++) {
        char ch = pg_three_cell(t, 0, r, c);
        CHECK(ch == '.' || ch == 'R' || ch == 'W' || ch == 'B');
        cnt[ch == 'R' ? 0 : ch == 'W' ? 1 : 2] += ch != '.';
        CHECK(pg_three_cell(t, 1, c, 2 - r) == ch);
        CHECK(pg_three_cell(t, 4, r, c) == ch);
        CHECK(pg_three_cell(t, 2, 2 - r, 2 - c) == ch);
      }
  CHECK(cnt[0] == 9 && cnt[1] == 9 && cnt[2] == 9);
  CHECK(pg_three_cell(0, 1, 2, 2) == 'R' && pg_three_cell(0, 0, 0, 2) == 'R');
  /* patterns: every cell has exactly two ends in every rotation, and four turns are the identity */
  for (g = PG_PAT5; g <= PG_PAT4; g++)
    for (t = 0; t < pg_ntiles(g); t++)
      for (r = 0; r < 4; r++)
        for (k = 0; k < 4; k++) {
          int m = pg_pat_mask(g, t, r, k), bits = (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1) + ((m >> 3) & 1);
          CHECK(bits == 2);
          CHECK(pg_pat_mask(g, t, r + 4, k) == m);
        }
  /* the first tile of Patterns 5x5 is NE SW / SW NE; turned once it is, cell by cell: BL (SW) -> TL turned (W,S -> N,W), ... */
  CHECK(pg_pat_mask(PG_PAT5, 0, 0, 0) == (PG_N | PG_E) && pg_pat_mask(PG_PAT5, 0, 0, 1) == (PG_S | PG_W));
  CHECK(pg_pat_mask(PG_PAT5, 0, 1, 0) == (PG_N | PG_W) && pg_pat_mask(PG_PAT5, 0, 1, 1) == (PG_E | PG_S));

  /* moves */
  for (g = PG_THREE; g <= PG_PAT4; g++) {
    pg_init(g, &s);
    CHECK(pg_placed(g, &s) == 0 && !pg_solved(g, &s) && pg_tile_check(g, &s) == 1);
    { PgMove m = {PGM_PLACE, 2, 1, 3}; CHECK(pg_apply(g, &s, m)); CHECK(pg_tile_slot(g, &s, 2) == 1 && s.rot[1] == 3 && pg_placed(g, &s) == 1); }
    { PgMove m = {PGM_PLACE, 2, 0, 0}; CHECK(!pg_apply(g, &s, m)); }                     /* the tile is already on the board */
    { PgMove m = {PGM_PLACE, 3, 1, 0}; CHECK(!pg_apply(g, &s, m)); }                     /* the slot is taken */
    { PgMove m = {PGM_PLACE, 3, 0, 4}; CHECK(!pg_apply(g, &s, m)); }                     /* no such rotation */
    { PgMove m = {PGM_PLACE, 99, 0, 0}; CHECK(!pg_apply(g, &s, m)); }
    { PgMove m = {PGM_TURN, 1, 3, 0}; CHECK(!pg_apply(g, &s, m)); }                      /* already turned that far */
    { PgMove m = {PGM_TURN, 0, 1, 0}; CHECK(!pg_apply(g, &s, m)); }                      /* empty slot */
    { PgMove m = {PGM_TURN, 1, 0, 0}; CHECK(pg_apply(g, &s, m)); CHECK(s.rot[1] == 0); }
    { PgMove m = {PGM_TAKE, 0, 0, 0}; CHECK(!pg_apply(g, &s, m)); }
    { PgMove m = {PGM_TAKE, 1, 0, 0}; CHECK(pg_apply(g, &s, m)); CHECK(pg_tile_slot(g, &s, 2) == -1 && pg_placed(g, &s) == 0); }
    { PgMove m = {PGM_SLIDE, 0, 1, 0}; CHECK(!pg_apply(g, &s, m)); }
  }
  PHASE(CHECK(three_count_solutions() == 16));
  PHASE(CHECK(pat_count_solutions(PG_PAT4) == 512));
  if (getenv("PG_LONG")) PHASE(CHECK(pat_count_solutions(PG_PAT5) == 2048)); /* the source counted 2048; the search is long: only on request (PG_LONG=1) */
}

/* count the solutions by depth-first search over the slots in reading order */
static PgState ts;
static int tcount;

static int line_cnt[20][3];

/* add (d = 1) or remove (d = -1) the circles of the tile in `slot`; returns 0 if some line then holds two of one colour */
static int three_put(int slot, int d) {
  int rr, cc, ok = 1;
  for (rr = 0; rr < 3; rr++)
    for (cc = 0; cc < 3; cc++) {
      char ch = pg_three_cell(ts.tile[slot], ts.rot[slot], rr, cc);
      int col = ch == 'R' ? 0 : ch == 'W' ? 1 : ch == 'B' ? 2 : -1, r = (slot / 3) * 3 + rr, c = (slot % 3) * 3 + cc;
      if (col < 0) continue;
      if ((line_cnt[r][col] += d) > 1) ok = 0;
      if ((line_cnt[9 + c][col] += d) > 1) ok = 0;
      if (r == c && (line_cnt[18][col] += d) > 1) ok = 0;
      if (r + c == 8 && (line_cnt[19][col] += d) > 1) ok = 0;
    }
  return ok;
}

static void three_dfs(int slot) {
  int t, r;
  if (slot == 9) { tcount += pg_solved(PG_THREE, &ts); return; }
  for (t = 0; t < 9; t++) {
    if (pg_tile_slot(PG_THREE, &ts, t) >= 0) continue;
    for (r = 0; r < 4; r++) {
      ts.tile[slot] = (signed char)t;
      ts.rot[slot] = (signed char)r;
      if (three_put(slot, 1)) three_dfs(slot + 1);
      three_put(slot, -1);
      ts.tile[slot] = -1;
      ts.rot[slot] = 0;
    }
  }
}

static int three_count_solutions(void) {
  memset(line_cnt, 0, sizeof line_cnt);
  pg_init(PG_THREE, &ts);
  tcount = 0;
  three_dfs(0);
  return tcount;
}

static int pat_g;

/* the line ends of the cells laid so far (row-major over the 2 * side square), kept up to date by the search */
static int pcell[100];

static void pat_set_cells(int slot, int on) {
  int side = pg_side(pat_g), cs = side * 2, k;
  for (k = 0; k < 4; k++)
    pcell[((slot / side) * 2 + k / 2) * cs + (slot % side) * 2 + k % 2] = on ? pg_pat_mask(pat_g, ts.tile[slot], ts.rot[slot], k) : 0;
}

/* a closed loop through one of the new tile's cells that does not cover the whole square can never become the one big loop */
static int pat_small_loop(int slot) {
  int side = pg_side(pat_g), cs = side * 2, total = cs * cs, k;
  for (k = 0; k < 4; k++) {
    int start = ((slot / side) * 2 + k / 2) * cs + (slot % side) * 2 + k % 2, cell = start, entered = 0, steps = 0;
    for (;;) {
      int ex = pcell[cell] & ~entered, dir, r = cell / cs, c = cell % cs;
      if (!ex) break;
      dir = ex & -ex;
      if (dir == PG_N) { if (r == 0) break; cell -= cs; entered = PG_S; }
      else if (dir == PG_S) { if (r == cs - 1) break; cell += cs; entered = PG_N; }
      else if (dir == PG_W) { if (c == 0) break; cell -= 1; entered = PG_E; }
      else { if (c == cs - 1) break; cell += 1; entered = PG_W; }
      steps++;
      if (!(pcell[cell] & entered)) break;        /* an open end: the line goes on later */
      if (cell == start) return steps < total;
      if (steps > total) break;
    }
  }
  return 0;
}

static void pat_dfs(int slot) {
  int side = pg_side(pat_g), t, r, k;
  if (slot == side * side) { tcount += pg_solved(pat_g, &ts); return; }
  for (t = 0; t < pg_ntiles(pat_g); t++) {
    if (pg_tile_slot(pat_g, &ts, t) >= 0) continue;
    for (r = 0; r < 4; r++) {
      int ok = 1, row = slot / side, col = slot % side;
      for (k = 0; k < 4 && ok; k++) { /* the joins of this tile with its left and upper neighbours and the border must fit */
        int m = pg_pat_mask(pat_g, t, r, k), cr = k / 2, cc = k % 2;
        if (cr == 0 && (m & PG_N)) {
          if (row == 0) ok = 0;
          else ok = ok && (pg_pat_mask(pat_g, ts.tile[slot - side], ts.rot[slot - side], 2 + cc) & PG_S) != 0;
        } else if (cr == 0 && row > 0 && (pg_pat_mask(pat_g, ts.tile[slot - side], ts.rot[slot - side], 2 + cc) & PG_S)) ok = 0;
        if (cc == 0 && (m & PG_W)) {
          if (col == 0) ok = 0;
          else ok = ok && (pg_pat_mask(pat_g, ts.tile[slot - 1], ts.rot[slot - 1], cr * 2 + 1) & PG_E) != 0;
        } else if (cc == 0 && col > 0 && (pg_pat_mask(pat_g, ts.tile[slot - 1], ts.rot[slot - 1], cr * 2 + 1) & PG_E)) ok = 0;
        if (cr == 1 && (m & PG_S) && row == side - 1) ok = 0;
        if (cc == 1 && (m & PG_E) && col == side - 1) ok = 0;
      }
      if (!ok) continue;
      ts.tile[slot] = (signed char)t;
      ts.rot[slot] = (signed char)r;
      pat_set_cells(slot, 1);
      if (!pat_small_loop(slot)) pat_dfs(slot + 1);
      pat_set_cells(slot, 0);
      ts.tile[slot] = -1;
      ts.rot[slot] = 0;
    }
  }
}

static int pat_count_solutions(int g) {
  pat_g = g;
  memset(pcell, 0, sizeof pcell);
  pg_init(g, &ts);
  tcount = 0;
  pat_dfs(0);
  return tcount;
}

/* ---- swap blocks ---- */

static void test_swap(void) {
  PgState s;
  int b, k, cells[8], steps, i;
  unsigned char dest[PG_SW_CELLS];
  pg_init(PG_SWAP, &s);
  /* 12 blocks of 8 cells fill 96 of the 128 cells, no overlap, and four rows in the middle are free */
  {
    int cover[PG_SW_CELLS], total = 0;
    memset(cover, 0, sizeof cover);
    for (b = 0; b < PG_BLOCKS; b++) { pg_swap_cells(b, &s, cells); for (k = 0; k < 8; k++) cover[cells[k]]++; }
    for (i = 0; i < PG_SW_CELLS; i++) { CHECK(cover[i] <= 1); total += cover[i]; }
    CHECK(total == 96);
    for (i = 6 * 8; i < 10 * 8; i++) CHECK(cover[i] == 0); /* the four middle rows (6..9, counting from 0) are free */
  }
  CHECK(pg_swap_shape(0) == 0 && pg_swap_shape(5) == 3 && pg_swap_colour(2) == 1 && pg_swap_colour(8) == 2 && pg_swap_colour(0) == 0);
  /* the anchor is the first covered cell in reading order and converts back to the origin */
  for (b = 0; b < PG_BLOCKS; b++) {
    int a = pg_swap_anchor(b, &s), first = 999;
    pg_swap_cells(b, &s, cells);
    for (k = 0; k < 8; k++) if (cells[k] < first) first = cells[k];
    CHECK(a == first);
    CHECK(pg_swap_origin_for_anchor(b, a) == s.org[b]);
    CHECK(pg_swap_block_at(&s, a) == b);
  }
  CHECK(pg_swap_origin_for_anchor(0, -1) == -1 && pg_swap_origin_for_anchor(0, 127) == -1);
  /* a move there and back is always possible; random play keeps the blocks apart and inside the box */
  srand(5);
  for (steps = 0; steps < 3000; steps++) {
    int n;
    b = rand() % PG_BLOCKS;
    n = pg_swap_dests(&s, b, dest);
    if (n) {
      int pick = rand() % n, o, from = s.org[b];
      unsigned char back[PG_SW_CELLS];
      PgMove m;
      for (o = 0; o < PG_SW_CELLS; o++) if (dest[o] && pick-- == 0) break;
      m.k = PGM_BLOCK; m.a = (unsigned char)b; m.b = (unsigned char)o; m.c = 0;
      CHECK(pg_apply(PG_SWAP, &s, m));
      pg_swap_dests(&s, b, back);
      CHECK(back[from]);
      {
        int cover[PG_SW_CELLS], bb;
        memset(cover, 0, sizeof cover);
        for (bb = 0; bb < PG_BLOCKS; bb++) {
          pg_swap_cells(bb, &s, cells);
          for (k = 0; k < 8; k++) { CHECK(cells[k] >= 0 && cells[k] < PG_SW_CELLS); CHECK(++cover[cells[k]] == 1); }
        }
      }
    }
  }
  { PgMove m = {PGM_BLOCK, 0, 0, 0}; pg_init(PG_SWAP, &s); CHECK(!pg_apply(PG_SWAP, &s, m)); }     /* the block's own origin is no destination */
  { PgMove m = {PGM_BLOCK, 12, 40, 0}; CHECK(!pg_apply(PG_SWAP, &s, m)); }
  { PgMove m = {PGM_BLOCK, 0, 200, 0}; CHECK(!pg_apply(PG_SWAP, &s, m)); }
  { PgMove m = {PGM_BLOCK, 0, 127, 0}; CHECK(!pg_apply(PG_SWAP, &s, m)); }                         /* the footprint would leave the box */
  /* the goal also counts with interchangeable blocks: blocks 0 and 6 (plain A) exchanged is still the start layout */
  pg_init(PG_SWAP, &s);
  { unsigned char t = s.org[0]; s.org[0] = s.org[6]; s.org[6] = t; }
  CHECK(!pg_solved(PG_SWAP, &s));
  pg_init(PG_SWAP, &s);
  { unsigned char t = s.org[2]; s.org[2] = s.org[8]; s.org[8] = t; t = s.org[3]; s.org[3] = s.org[9]; s.org[9] = t; }
  CHECK(pg_solved(PG_SWAP, &s));
  { unsigned char t = s.org[1]; s.org[1] = s.org[7]; s.org[7] = t; } /* blocks 1 and 7 are both plain B: interchangeable */
  CHECK(pg_solved(PG_SWAP, &s));
}

static void test_misc(void) {
  PgMove m = {PGM_PLACE, 24, 3, 2}, u;
  PgState s;
  PgMove list[3] = {{PGM_PLACE, 0, 0, 1}, {PGM_TURN, 0, 2, 0}, {PGM_TAKE, 0, 0, 0}};
  CHECK(pg_pack(m) == ((1u << 24) | (24u << 16) | (3u << 8) | 2u));
  u = pg_unpack(pg_pack(m));
  CHECK(u.k == m.k && u.a == m.a && u.b == m.b && u.c == m.c);
  CHECK(pg_replay(PG_THREE, &s, list, 3) == 3 && pg_placed(PG_THREE, &s) == 0);
  list[1].b = 1; /* turn to the rotation it already has */
  CHECK(pg_replay(PG_THREE, &s, list, 3) == 1);
  CHECK(pg_kind(-1) == -1 && pg_kind(PG_GAMES) == -1 && !strcmp(pg_id(99), ""));
  CHECK(pg_par(PG_EIGHT) == 28 && pg_par(PG_CATS) == 32 && pg_par(PG_HOUR) == 26 && pg_par(PG_SWAP) == 32 && pg_par(PG_THREE) == 0);
}

int main(void) {
  PHASE(test_solutions());
  PHASE(test_graph_rules());
  PHASE(test_swap());
  PHASE(test_misc());
  PHASE(CHECK(bfs_par(PG_CATS) == 32));
  PHASE(CHECK(bfs_par(PG_EIGHT) == 28));
  PHASE(test_tile_rules());
  printf("playground core: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
