#include "rebus_core.h"

#include <stdlib.h>
#include <string.h>

const char *puzzle_number(const Puzzle *p, int row, int slot) {
  if (row <= 3) return slot <= 3 ? p->rows[row].nums[slot] : p->rows[row].result;
  return slot <= 3 ? p->rows[slot].result : p->total;
}

void grid_init(Grid *g, const Puzzle *p) {
  int r, s, k;
  memset(g, 0, sizeof(*g));
  g->p = p;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++) {
      g->width[r][s] = (uint8_t)strlen(puzzle_number(p, r, s));
      for (k = 0; k < MAXPOS; k++) g->digit[r][s][k] = -1;
    }
  for (k = 0; k < p->ngivens; k++) {
    const int *e = p->givens[k];
    g->digit[e[0]][e[1]][e[2]] = (int8_t)e[3];
    g->given[e[0]][e[1]][e[2]] = 1;
  }
}

static int cell_ok(const Grid *g, int row, int slot, int pos) {
  return row >= 0 && row < 5 && slot >= 0 && slot < 5 && pos >= 0 && pos < g->width[row][slot];
}

int grid_set(Grid *g, int row, int slot, int pos, int digit) {
  if (!cell_ok(g, row, slot, pos) || digit < 0 || digit > 9 || g->given[row][slot][pos]) return 0;
  g->digit[row][slot][pos] = (int8_t)digit;
  return 1;
}

int grid_clear(Grid *g, int row, int slot, int pos) {
  if (!cell_ok(g, row, slot, pos) || g->given[row][slot][pos]) return 0;
  g->digit[row][slot][pos] = -1;
  return 1;
}

void grid_reset(Grid *g) {
  int r, s, k;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++)
      for (k = 0; k < g->width[r][s]; k++) grid_clear(g, r, s, k);
}

int grid_complete(const Grid *g) {
  int r, s, k;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++)
      for (k = 0; k < g->width[r][s]; k++)
        if (g->digit[r][s][k] < 0) return 0;
  return 1;
}

/* Value of the number at (row, slot) if fully filled, else -1. *lz is set when it
 * is zero or has a leading zero. */
static long number_at(const Grid *g, int row, int slot, int *lz) {
  long v = 0;
  int k;
  for (k = 0; k < g->width[row][slot]; k++) {
    if (g->digit[row][slot][k] < 0) return -1;
    v = v * 10 + g->digit[row][slot][k];
  }
  if (lz) *lz = (v == 0) || (g->width[row][slot] > 1 && g->digit[row][slot][0] == 0);
  return v;
}

int eval_row(const long nums[4], const char *ops, long *out) {
  long acc = nums[0];
  int i;
  for (i = 0; i < 3; i++) {
    long n = nums[i + 1];
    switch (ops[i]) {
      case '+': acc += n; break;
      case '-': acc -= n; break;
      case '*': acc *= n; break;
      case ':':
        if (n == 0 || acc % n != 0) return -1;
        acc /= n;
        break;
      default: return -1;
    }
  }
  *out = acc;
  return 0;
}

static void mark_number(Marks *m, const Grid *g, int row, int slot) {
  int k;
  for (k = 0; k < g->width[row][slot]; k++) m->bad[row][slot][k] = 1;
}

int grid_violations(const Grid *g, Marks *m) {
  int r, s, j;
  long v[5][5];
  int lz;
  memset(m, 0, sizeof(*m));

  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++) {
      lz = 0;
      v[r][s] = number_at(g, r, s, &lz);
      if (v[r][s] >= 0 && lz) {
        mark_number(m, g, r, s);
        m->count++;
      }
    }

  for (r = 0; r < 4; r++) { /* row equations */
    long nums[4], res;
    int full = v[r][4] >= 0;
    for (s = 0; s < 4; s++) {
      nums[s] = v[r][s];
      if (nums[s] < 0) full = 0;
    }
    if (!full) continue;
    if (eval_row(nums, g->p->rows[r].ops, &res) != 0 || res != v[r][4]) {
      for (s = 0; s < 5; s++) mark_number(m, g, r, s);
      m->count++;
    }
  }

  for (j = 0; j < 4; j++) { /* column sums */
    long sum = 0;
    int full = v[j][4] >= 0;
    for (r = 0; r < 4; r++) {
      if (v[r][j] < 0) full = 0;
      sum += v[r][j];
    }
    if (!full || sum == v[j][4]) continue;
    for (r = 0; r < 4; r++) mark_number(m, g, r, j);
    mark_number(m, g, j, 4);
    m->count++;
  }

  for (j = 0; j < 4; j++) { /* summary box must equal that row's result */
    if (v[4][j] >= 0 && v[j][4] >= 0 && v[4][j] != v[j][4]) {
      mark_number(m, g, 4, j);
      mark_number(m, g, j, 4);
      m->count++;
    }
  }

  { /* grand total */
    long sum = 0;
    int full = v[4][4] >= 0;
    for (j = 0; j < 4; j++) {
      if (v[4][j] < 0) full = 0;
      sum += v[4][j];
    }
    if (full && sum != v[4][4]) {
      for (j = 0; j < 5; j++) mark_number(m, g, 4, j);
      m->count++;
    }
  }
  return m->count;
}

int grid_wins(const Grid *g) {
  Marks m;
  return grid_complete(g) && grid_violations(g, &m) == 0;
}

void grid_save(const Grid *g, char *out) {
  int r, s, k, n = 0;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++)
      for (k = 0; k < MAXPOS; k++) {
        int d = (k < g->width[r][s]) ? g->digit[r][s][k] : -1;
        out[n++] = d < 0 ? '.' : (char)('0' + d);
      }
  out[n] = 0;
}

void grid_load(Grid *g, const char *s) {
  int r, c, k, n = 0;
  if (!s || (int)strlen(s) < GRID_SAVE_LEN) return;
  for (r = 0; r < 5; r++)
    for (c = 0; c < 5; c++)
      for (k = 0; k < MAXPOS; k++, n++)
        if (s[n] >= '0' && s[n] <= '9') grid_set(g, r, c, k, s[n] - '0');
}
