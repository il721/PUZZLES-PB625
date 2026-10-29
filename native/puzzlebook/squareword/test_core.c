/* Host test for squareword_core: gcc -O2 -o /tmp/t test_core.c squareword_core.c && /tmp/t */
#include <stdio.h>
#include <string.h>

#include "squareword_core.h"
#include "squareword_data.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* backtracking solver: first empty cell in reading order, every letter that does not repeat in its row, column or
 * diagonal; counts solutions up to a cap and keeps the first one */
static SwBoard sb;
static int nsol, cap;
static int8_t first[SW_MAX][SW_MAX];

static int ok_here(int r, int c, int l) {
  int n = sb.p->n, k;
  for (k = 0; k < n; k++) {
    if (k != c && sb.cell[r][k] == l) return 0;
    if (k != r && sb.cell[k][c] == l) return 0;
    if (r == c && k != r && sb.cell[k][k] == l) return 0;
    if (r + c == n - 1 && k != r && sb.cell[k][n - 1 - k] == l) return 0;
  }
  return 1;
}

static void search(int pos) {
  int n = sb.p->n, r, c, l;
  if (nsol >= cap) return;
  while (pos < n * n && sb.cell[pos / n][pos % n] >= 0) pos++;
  if (pos == n * n) {
    if (nsol == 0) memcpy(first, sb.cell, sizeof first);
    nsol++;
    return;
  }
  r = pos / n; c = pos % n;
  for (l = 0; l < n; l++) {
    if (!ok_here(r, c, l)) continue;
    sb.cell[r][c] = (int8_t)l;
    search(pos + 1);
    sb.cell[r][c] = -1;
    if (nsol >= cap) return;
  }
}

static int solve(const SwPuzzle *p, int c) {
  sw_init(&sb, p);
  nsol = 0; cap = c;
  search(0);
  return nsol;
}

int main(void) {
  int i, r, c, k, total = 0;
  CHECK(SNUM_PUZZLES == 18);
  CHECK(SPUZZLES[0].tutorial && !SPUZZLES[1].tutorial);

  for (i = 0; i < SNUM_PUZZLES; i++) {
    const SwPuzzle *p = &SPUZZLES[i];
    int n = p->n, nsols, ngiven = 0;
    SwBoard b;
    SwStatus st;
    sw_init(&b, p);
    CHECK(n >= 5 && n <= 7);
    for (k = 0; k < n; k++) {
      int j;
      CHECK(p->kw[k] >= 0x0410 && p->kw[k] <= 0x044F);       /* Cyrillic capitals */
      for (j = 0; j < k; j++) CHECK(p->kw[j] != p->kw[k]);
      CHECK(p->given[0][k] == k);                                /* row 0 is the keyword */
    }
    for (r = 0; r < n; r++)
      for (c = 0; c < n; c++) {
        CHECK(p->given[r][c] >= -1 && p->given[r][c] < n);
        if (p->given[r][c] >= 0) ngiven++;
        CHECK(sw_is_given(&b, r, c) == (p->given[r][c] >= 0));
      }
    CHECK(ngiven == (n == 5 ? 8 : n == 6 ? 12 : 16));            /* keyword + the secondary words (3 letters each) */
    sw_status(&b, &st);
    CHECK(st.filled == ngiven && st.violations == 0 && !st.solved);
    /* the book's givens never repeat a letter in a line, and the puzzle has a solution */
    nsols = solve(p, 3);
    CHECK(nsols >= 1);
    total += nsols;
    printf("puzzle %-8s n=%d givens=%2d solutions(cap 3)=%d\n", p->id, n, ngiven, nsols);
    /* play the first solution in: wins exactly then */
    sw_init(&b, p);
    for (r = 0; r < n; r++)
      for (c = 0; c < n; c++) {
        if (sw_is_given(&b, r, c)) continue;
        CHECK(!sw_wins(&b));                                     /* this cell is still empty */
        CHECK(sw_set(&b, r, c, first[r][c]));
      }
    sw_status(&b, &st);
    CHECK(st.solved && st.filled == n * n && st.violations == 0 && sw_wins(&b));
    for (k = 0; k < n; k++) CHECK(st.count[k] == n);
    /* save/load round trip and list */
    {
      char s[SW_SAVE_LEN + 1];
      uint8_t l[SW_CELLS][3];
      SwBoard b2;
      int cnt = sw_list(&b, l);
      CHECK(cnt == n * n - ngiven);
      sw_save(&b, s);
      CHECK((int)strlen(s) == n * n);
      sw_init(&b2, p);
      sw_load(&b2, s);
      CHECK(memcmp(b.cell, b2.cell, sizeof b.cell) == 0);
      CHECK(sw_wins(&b2));
    }
    /* a player cell can be swapped to a repeat: both repeating cells are flagged, the win is gone */
    for (r = 1; r < n; r++) {
      int cc = -1;
      for (c = 0; c < n; c++) if (!sw_is_given(&b, r, c)) { cc = c; break; }
      if (cc < 0) continue;
      {
        int8_t old = b.cell[r][cc], other = b.cell[r][(cc + 1) % n];
        CHECK(sw_set(&b, r, cc, other));
        sw_status(&b, &st);
        CHECK(!st.solved && st.violations >= 2 && st.bad[r][cc] && st.bad[r][(cc + 1) % n]);
        CHECK(sw_set(&b, r, cc, -1));
        sw_status(&b, &st);
        CHECK(!st.solved && st.filled == n * n - 1);
        CHECK(sw_set(&b, r, cc, old));
        CHECK(sw_wins(&b));
      }
      break;
    }
    /* givens and out-of-range targets are untouchable */
    CHECK(!sw_set(&b, 0, 0, 1) && b.cell[0][0] == 0);
    CHECK(!sw_set(&b, n, 0, 0) && !sw_set(&b, 0, n, 0) && !sw_set(&b, -1, 0, 0));
    CHECK(!sw_set(&b, 1, 0, n) && !sw_set(&b, 1, 0, -2));
    /* reset returns to the printed cells */
    sw_reset(&b);
    sw_status(&b, &st);
    CHECK(st.filled == ngiven && b.sel_r == -1);
  }
  printf("solutions found (capped): %d\n", total);

  /* diagonals count: the same letter twice on the main diagonal and on the anti-diagonal */
  {
    SwBoard b;
    SwStatus st;
    const SwPuzzle *p = &SPUZZLES[1];
    int n = p->n, d = -1, e = -1, f = -1;
    sw_init(&b, p);
    for (k = 1; k < n && (d < 0 || e < 0); k++) {
      if (!sw_is_given(&b, k, k) && d < 0) d = k;
      else if (!sw_is_given(&b, k, k) && e < 0) e = k;
    }
    CHECK(d > 0 && e > 0);
    sw_set(&b, d, d, 1); sw_set(&b, e, e, 1);
    sw_status(&b, &st);
    CHECK(st.bad[d][d] && st.bad[e][e]);
    sw_reset(&b);
    for (k = 1; k < n; k++) if (!sw_is_given(&b, k, n - 1 - k)) { if (f < 0) f = k; }
    CHECK(f > 0);
    sw_set(&b, f, n - 1 - f, 4);
    sw_status(&b, &st);
    /* letter 4 is the last keyword letter: row 0 holds it at (0, n-1), which is on the anti-diagonal */
    CHECK(st.bad[f][n - 1 - f] && st.bad[0][n - 1]);
  }

  /* load is tolerant */
  {
    SwBoard b;
    char junk[SW_SAVE_LEN + 1];
    SwStatus st;
    sw_init(&b, &SPUZZLES[1]);
    memset(junk, 'x', sizeof junk); junk[SW_SAVE_LEN] = 0;
    sw_load(&b, junk);
    sw_status(&b, &st);
    CHECK(st.filled == 8);
    memset(junk, '9', sizeof junk);
    sw_load(&b, junk);
    sw_status(&b, &st);
    CHECK(st.filled == 8);
    sw_load(&b, "");
    sw_status(&b, &st);
    CHECK(st.filled == 8);
    sw_load(&b, "0000");
    sw_status(&b, &st);
    CHECK(st.filled == 8);    /* row 0 holds givens only */
  }

  printf("%d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
