#include <string.h>

#include "digit_core.h"

/* digits admitted by G1..G5 (book p. 31): {0,8,9} {0,6,8} {1,4,7} {2,3} {3,5,9} */
static const unsigned GLYPH_DIGITS[6] = {
  0,
  (1u << 0) | (1u << 8) | (1u << 9),
  (1u << 0) | (1u << 6) | (1u << 8),
  (1u << 1) | (1u << 4) | (1u << 7),
  (1u << 2) | (1u << 3),
  (1u << 3) | (1u << 5) | (1u << 9),
};

int glyph_admits(int g, int d) { return g >= 1 && g <= 5 && d >= 0 && d <= 9 && (GLYPH_DIGITS[g] >> d & 1u); }

void dgrid_init(DGrid *g, const DPuzzle *p) {
  g->p = p;
  memset(g->d, -1, sizeof g->d);
}

int dgrid_set(DGrid *g, int r, int c, int digit) {
  if (r < 0 || r > 3 || c < 0 || c > 3 || digit < 0 || digit > 9) return 0;
  if (g->d[r][c] == digit) return 0;
  g->d[r][c] = (int8_t)digit;
  return 1;
}

int dgrid_clear(DGrid *g, int r, int c) {
  if (r < 0 || r > 3 || c < 0 || c > 3 || g->d[r][c] < 0) return 0;
  g->d[r][c] = -1;
  return 1;
}

void dgrid_reset(DGrid *g) { memset(g->d, -1, sizeof g->d); }

int dgrid_complete(const DGrid *g) {
  int r, c;
  for (r = 0; r < 4; r++)
    for (c = 0; c < 4; c++)
      if (g->d[r][c] < 0) return 0;
  return 1;
}

int deval_line(const int cells[3], const char *ops) {
  int acc = cells[0], i;
  for (i = 0; i < 2; i++) {
    int next = cells[i + 1];
    switch (ops[i]) {
      case '+': acc += next; break;
      case '-': acc -= next; if (acc < 0) return -1; break;
      case '*': acc *= next; break;
      case ':': if (next == 0 || acc % next != 0) return -1; acc /= next; break;
      default: return -1;
    }
  }
  return acc;
}

static int line_holds(const int cells[4], const char *ops) {
  int v = deval_line(cells, ops);
  return v >= 0 && v == cells[3];
}

int dgrid_violations(const DGrid *g, DMarks *m) {
  int r, c, i;
  memset(m, 0, sizeof *m);
  for (r = 0; r < 4; r++)
    for (c = 0; c < 4; c++)
      if (g->d[r][c] >= 0 && !glyph_admits(g->p->glyph[r][c], g->d[r][c])) { m->bad[r][c] = 1; m->count++; }
  for (r = 0; r < 4; r++) {
    int line[4], full = 1;
    for (c = 0; c < 4; c++) { line[c] = g->d[r][c]; if (line[c] < 0) full = 0; }
    if (full && !line_holds(line, g->p->rowops[r])) { for (c = 0; c < 4; c++) m->bad[r][c] = 1; m->count++; }
  }
  for (c = 0; c < 4; c++) {
    int line[4], full = 1;
    for (i = 0; i < 4; i++) { line[i] = g->d[i][c]; if (line[i] < 0) full = 0; }
    if (full && !line_holds(line, g->p->colops[c])) { for (i = 0; i < 4; i++) m->bad[i][c] = 1; m->count++; }
  }
  return m->count;
}

int dgrid_wins(const DGrid *g) {
  DMarks m;
  return dgrid_complete(g) && dgrid_violations(g, &m) == 0;
}

void dgrid_save(const DGrid *g, char *out) {
  int i;
  for (i = 0; i < DGRID_SAVE_LEN; i++) {
    int v = g->d[i / 4][i % 4];
    out[i] = v < 0 ? '.' : (char)('0' + v);
  }
  out[DGRID_SAVE_LEN] = 0;
}

void dgrid_load(DGrid *g, const char *s) {
  int i;
  dgrid_reset(g);
  for (i = 0; i < DGRID_SAVE_LEN && s[i]; i++)
    if (s[i] >= '0' && s[i] <= '9') g->d[i / 4][i % 4] = (int8_t)(s[i] - '0');
}
