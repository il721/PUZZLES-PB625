/* Squarewords rules; see squareword_core.h. */
#include <string.h>

#include "squareword_core.h"

void sw_reset(SwBoard *b) {
  int r, c, n = b->p->n;
  memset(b->cell, -1, sizeof b->cell);
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++) b->cell[r][c] = b->p->given[r][c];
  b->sel_r = -1;
  b->sel_c = -1;
}

void sw_init(SwBoard *b, const SwPuzzle *p) {
  b->p = p;
  sw_reset(b);
}

int sw_is_given(const SwBoard *b, int r, int c) { return b->p->given[r][c] >= 0; }

int sw_set(SwBoard *b, int r, int c, int letter) {
  int n = b->p->n;
  if (r < 0 || r >= n || c < 0 || c >= n || letter < -1 || letter >= n || sw_is_given(b, r, c)) return 0;
  b->cell[r][c] = (int8_t)letter;
  return 1;
}

/* cell k (0..n-1) of line `line`: 0..n-1 rows, n..2n-1 columns, 2n the main diagonal, 2n+1 the anti-diagonal */
static void line_cell(int n, int line, int k, int *r, int *c) {
  if (line < n) { *r = line; *c = k; }
  else if (line < 2 * n) { *r = k; *c = line - n; }
  else if (line == 2 * n) { *r = k; *c = k; }
  else { *r = k; *c = n - 1 - k; }
}

void sw_status(const SwBoard *b, SwStatus *st) {
  int n = b->p->n, r, c, l, k, line;
  memset(st, 0, sizeof *st);
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++) {
      int v = b->cell[r][c];
      if (v < 0) continue;
      st->filled++;
      st->count[v]++;
    }
  /* a letter seen twice in a line flags every cell of it holding that letter */
  for (line = 0; line < 2 * n + 2; line++)
    for (l = 0; l < n; l++) {
      int seen = 0;
      for (k = 0; k < n; k++) {
        line_cell(n, line, k, &r, &c);
        seen += b->cell[r][c] == l;
      }
      if (seen < 2) continue;
      for (k = 0; k < n; k++) {
        line_cell(n, line, k, &r, &c);
        if (b->cell[r][c] == l) st->bad[r][c] = 1;
      }
    }
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++) st->violations += st->bad[r][c];
  st->solved = st->filled == n * n && st->violations == 0;
}

int sw_wins(const SwBoard *b) {
  SwStatus st;
  sw_status(b, &st);
  return st.solved;
}

int sw_list(const SwBoard *b, uint8_t out[SW_CELLS][3]) {
  int n = b->p->n, r, c, k = 0;
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++)
      if (!sw_is_given(b, r, c) && b->cell[r][c] >= 0) {
        out[k][0] = (uint8_t)r; out[k][1] = (uint8_t)c; out[k][2] = (uint8_t)b->cell[r][c];
        k++;
      }
  return k;
}

void sw_save(const SwBoard *b, char *out) {
  int n = b->p->n, r, c, k = 0;
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++) out[k++] = (sw_is_given(b, r, c) || b->cell[r][c] < 0) ? '.' : (char)('0' + b->cell[r][c]);
  out[k] = 0;
}

void sw_load(SwBoard *b, const char *s) {
  int n = b->p->n, r, c, k = 0;
  sw_reset(b);
  for (r = 0; r < n; r++)
    for (c = 0; c < n; c++, k++) {
      if (!s[k]) return;
      if (s[k] >= '0' && s[k] < '0' + n) sw_set(b, r, c, s[k] - '0');
    }
}
