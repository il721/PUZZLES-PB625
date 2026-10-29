#include <string.h>

#include "domino_core.h"

static int idx(int r, int c) { return r * DCOLS + c; }
static int in_range(int r, int c) { return r >= 0 && r < DROWS && c >= 0 && c < DCOLS; }

void dom_reset(DomBoard *b) {
  memset(b->partner, -1, sizeof b->partner);
  b->pending = -1;
}

void dom_init(DomBoard *b, const DomPuzzle *p) {
  b->p = p;
  dom_reset(b);
}

int dom_place(DomBoard *b, int r1, int c1, int r2, int c2) {
  int a, d, dr = r1 - r2, dc = c1 - c2;
  if (!in_range(r1, c1) || !in_range(r2, c2)) return 0;
  if ((dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc) != 1) return 0;
  a = idx(r1, c1); d = idx(r2, c2);
  if (b->partner[a] >= 0 || b->partner[d] >= 0) return 0;
  b->partner[a] = (int8_t)d;
  b->partner[d] = (int8_t)a;
  return 1;
}

int dom_tap(DomBoard *b, int r, int c) {
  int i, p;
  if (!in_range(r, c)) return 0;
  i = idx(r, c);
  if (b->partner[i] >= 0) { /* a placed domino: take it back, whatever is selected */
    b->partner[b->partner[i]] = -1;
    b->partner[i] = -1;
    b->pending = -1;
    return 1;
  }
  p = b->pending;
  if (p < 0) b->pending = (int8_t)i;
  else if (p == i) b->pending = -1;
  else if (dom_place(b, p / DCOLS, p % DCOLS, r, c)) { b->pending = -1; return 1; }
  else b->pending = (int8_t)i; /* not adjacent: the selection moves */
  return 0;
}

void dom_status(const DomBoard *b, DomStatus *st) {
  int i;
  memset(st, 0, sizeof *st);
  for (i = 0; i < DCELLS; i++) {
    int j = b->partner[i], v1, v2;
    if (j < 0) continue;
    st->covered++;
    if (j < i) continue; /* count each domino once, at its first cell */
    st->placed++;
    v1 = b->p->grid[i / DCOLS][i % DCOLS];
    v2 = b->p->grid[j / DCOLS][j % DCOLS];
    st->count[v1 < v2 ? v1 : v2][v1 < v2 ? v2 : v1]++;
  }
  for (i = 0; i < DCELLS; i++) {
    int j = b->partner[i], v1, v2;
    if (j < 0) continue;
    v1 = b->p->grid[i / DCOLS][i % DCOLS];
    v2 = b->p->grid[j / DCOLS][j % DCOLS];
    if (st->count[v1 < v2 ? v1 : v2][v1 < v2 ? v2 : v1] > 1) { st->conflict[i] = 1; st->nconflicts++; }
  }
  st->solved = st->covered == DCELLS && st->nconflicts == 0;
}

int dom_wins(const DomBoard *b) {
  DomStatus st;
  dom_status(b, &st);
  return st.solved;
}

int dom_list(const DomBoard *b, uint8_t out[DDOMS][4]) {
  int i, n = 0;
  for (i = 0; i < DCELLS; i++) {
    int j = b->partner[i];
    if (j < i) continue; /* free (-1) or the second half */
    out[n][0] = (uint8_t)(i / DCOLS); out[n][1] = (uint8_t)(i % DCOLS);
    out[n][2] = (uint8_t)(j / DCOLS); out[n][3] = (uint8_t)(j % DCOLS);
    n++;
  }
  return n;
}

void dom_save(const DomBoard *b, char *out) {
  int i;
  for (i = 0; i < DCELLS; i++) {
    int j = b->partner[i];
    out[i] = (j > i) ? ((j == i + 1) ? 'R' : 'D') : '.';
  }
  out[DCELLS] = 0;
}

void dom_load(DomBoard *b, const char *s) {
  int i;
  dom_reset(b);
  for (i = 0; i < DCELLS && s[i] && s[i] != '\n'; i++) {
    int r = i / DCOLS, c = i % DCOLS;
    if (s[i] == 'R') dom_place(b, r, c, r, c + 1);
    else if (s[i] == 'D') dom_place(b, r, c, r + 1, c);
  }
}
