/* Host test for domino_core: gcc -o /tmp/t test_core.c domino_core.c && /tmp/t */
#include <stdio.h>
#include <string.h>

#include "domino_core.h"
#include "domino_data.h"
#include "domino_tutorial.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* exhaustive solver (the source's backtracking): first free cell in reading order, every free neighbour whose pair is unused */
static const DomPuzzle *sp;
static int8_t cov[DCELLS];
static int usedp[7][7], nsol, cap;
static int8_t first[DDOMS][4], cur[DDOMS][4];
static int depth;
static void search(void) {
  static const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
  int i, k;
  if (nsol >= cap) return;
  for (i = 0; i < DCELLS && cov[i]; i++) {}
  if (i == DCELLS) {
    if (nsol == 0) memcpy(first, cur, sizeof first);
    nsol++;
    return;
  }
  for (k = 0; k < 4; k++) {
    int r = i / DCOLS + dr[k], c = i % DCOLS + dc[k], j, a, b;
    if (r < 0 || r >= DROWS || c < 0 || c >= DCOLS) continue;
    j = r * DCOLS + c;
    if (cov[j]) continue;
    a = sp->grid[i / DCOLS][i % DCOLS]; b = sp->grid[r][c];
    if (a > b) { int t = a; a = b; b = t; }
    if (usedp[a][b]) continue;
    cov[i] = cov[j] = 1; usedp[a][b] = 1;
    cur[depth][0] = (int8_t)(i / DCOLS); cur[depth][1] = (int8_t)(i % DCOLS); cur[depth][2] = (int8_t)r; cur[depth][3] = (int8_t)c;
    depth++;
    search();
    depth--;
    cov[i] = cov[j] = 0; usedp[a][b] = 0;
    if (nsol >= cap) return;
  }
}
static int solve(const DomPuzzle *p, int c) {
  sp = p; cap = c; nsol = 0; depth = 0;
  memset(cov, 0, sizeof cov); memset(usedp, 0, sizeof usedp);
  search();
  return nsol;
}

int main(void) {
  int i, r, c, k;
  DomBoard b, b2;
  DomStatus st;
  char s[DOM_SAVE_LEN + 1];
  uint8_t list[DDOMS][4];

  CHECK(DNUM_PUZZLES == 19 && DPUZZLES[0].tutorial && !DPUZZLES[1].tutorial);

  /* tap state machine on the sample: free cell selects, second tap on a neighbour binds, tap on a domino removes */
  dom_init(&b, &DPUZZLES[0]);
  CHECK(!dom_tap(&b, 0, 0) && b.pending == 0);
  CHECK(!dom_tap(&b, 0, 0) && b.pending == -1);            /* deselect */
  dom_tap(&b, 0, 0);
  CHECK(!dom_tap(&b, 3, 3) && b.pending == 3 * DCOLS + 3); /* not adjacent: selection moves */
  CHECK(dom_tap(&b, 3, 4) && b.pending == -1);             /* bound */
  CHECK(b.partner[3 * DCOLS + 3] == 3 * DCOLS + 4 && b.partner[3 * DCOLS + 4] == 3 * DCOLS + 3);
  dom_tap(&b, 0, 0);
  CHECK(dom_tap(&b, 3, 3) && b.pending == -1 && b.partner[3 * DCOLS + 3] == -1 && b.partner[3 * DCOLS + 4] == -1); /* removal clears the selection */
  dom_reset(&b);
  CHECK(!dom_tap(&b, -1, 0) && !dom_tap(&b, 0, 7) && !dom_tap(&b, 8, 0));
  /* a selected cell cannot be bound with a covered neighbour: tapping the covered one removes instead */
  dom_tap(&b, 0, 0); dom_tap(&b, 0, 1); /* 0:6 at (0,0)-(0,1) */
  dom_tap(&b, 1, 0);
  CHECK(b.pending == DCOLS);
  CHECK(dom_tap(&b, 0, 0) && b.pending == -1 && b.partner[0] == -1 && b.partner[1] == -1);

  /* place / validation */
  dom_reset(&b);
  CHECK(dom_place(&b, 0, 0, 0, 1) && !dom_place(&b, 0, 1, 0, 2) && !dom_place(&b, 0, 0, 1, 0));
  CHECK(!dom_place(&b, 0, 2, 0, 4) && !dom_place(&b, 0, 2, 1, 3) && !dom_place(&b, 0, 6, 0, 7) && !dom_place(&b, -1, 0, 0, 0) && !dom_place(&b, 7, 6, 8, 6));
  CHECK(!dom_place(&b, 1, 1, 1, 1));

  /* status: duplicates are flagged from the moment both dominoes are on the board; a partial board is never solved */
  dom_reset(&b);
  /* example grid row 0: 0 6 2 5 0 0 5 ; row 1: 0 4 3 2 6 6 2. (0,4)-(0,5) is 0:0, (1,0)-(0,0) is 0:0 -> duplicate */
  dom_place(&b, 0, 4, 0, 5);
  dom_status(&b, &st);
  CHECK(st.placed == 1 && st.covered == 2 && st.count[0][0] == 1 && st.nconflicts == 0 && !st.solved);
  dom_place(&b, 0, 0, 1, 0);
  dom_status(&b, &st);
  CHECK(st.placed == 2 && st.count[0][0] == 2 && st.nconflicts == 4 && st.conflict[4] && st.conflict[5] && st.conflict[0] && st.conflict[DCOLS]);
  CHECK(!st.solved);
  dom_tap(&b, 0, 0); /* remove the second one: conflict gone */
  dom_status(&b, &st);
  CHECK(st.nconflicts == 0 && st.placed == 1);

  /* order of the cells does not matter for a value pair: 3:5 == 5:3 */
  dom_reset(&b);
  dom_place(&b, 4, 4, 4, 3); /* 3 5? grid row 4: 0 2 2 5 3 4 4 -> (4,3)=5,(4,4)=3 */
  dom_status(&b, &st);
  CHECK(st.count[3][5] == 1 && st.count[5][3] == 0);

  /* every puzzle: 8 of every digit; the sample's printed solution is a valid tiling with the full set */
  for (i = 0; i < DNUM_PUZZLES; i++) {
    int cnt[7] = {0};
    const DomPuzzle *p = &DPUZZLES[i];
    for (r = 0; r < DROWS; r++) for (c = 0; c < DCOLS; c++) cnt[p->grid[r][c]]++;
    for (k = 0; k < 7; k++) CHECK(cnt[k] == 8);
    CHECK(p->nsol == (p->tutorial ? 28 : 0));
  }
  dom_init(&b, &DPUZZLES[0]);
  for (k = 0; k < DPUZZLES[0].nsol; k++) CHECK(dom_place(&b, DPUZZLES[0].sol[k][0], DPUZZLES[0].sol[k][1], DPUZZLES[0].sol[k][2], DPUZZLES[0].sol[k][3]));
  dom_status(&b, &st);
  CHECK(st.placed == 28 && st.covered == 56 && st.nconflicts == 0 && st.solved && dom_wins(&b));
  for (r = 0; r < 7; r++) for (c = r; c < 7; c++) CHECK(st.count[r][c] == 1);

  /* every puzzle has a solution, found by the exhaustive solver; it is a win on the board. Uniqueness is reported. */
  for (i = 0; i < DNUM_PUZZLES; i++) {
    int n = solve(&DPUZZLES[i], 2);
    CHECK(n >= 1);
    if (n != 1) printf("note: %s has %d+ solutions\n", DPUZZLES[i].id, n);
    dom_init(&b, &DPUZZLES[i]);
    for (k = 0; k < DDOMS; k++) CHECK(dom_place(&b, first[k][0], first[k][1], first[k][2], first[k][3]));
    CHECK(dom_wins(&b));
    /* drop one domino: no longer solved; put it back in the other orientation of nothing: stays solved only when restored */
    dom_tap(&b, first[5][0], first[5][1]);
    CHECK(!dom_wins(&b));
    dom_place(&b, first[5][0], first[5][1], first[5][2], first[5][3]);
    CHECK(dom_wins(&b));
  }

  /* the sample's printed solution is the unique one */
  CHECK(solve(&DPUZZLES[0], 2) == 1);

  /* save / load round trip, list order, tolerant load */
  dom_init(&b, &DPUZZLES[0]);
  for (k = 0; k < 28; k++) dom_place(&b, DPUZZLES[0].sol[k][0], DPUZZLES[0].sol[k][1], DPUZZLES[0].sol[k][2], DPUZZLES[0].sol[k][3]);
  dom_tap(&b, 2, 2); /* remove the domino covering (2,2) */
  dom_save(&b, s);
  CHECK(strlen(s) == DOM_SAVE_LEN);
  dom_init(&b2, &DPUZZLES[0]);
  dom_load(&b2, s);
  CHECK(memcmp(b.partner, b2.partner, sizeof b.partner) == 0);
  k = dom_list(&b, list);
  CHECK(k == dom_list(&b2, list) && k >= 26);
  {
    uint8_t l1[DDOMS][4];
    int n1 = dom_list(&b, l1), j;
    for (j = 1; j < n1; j++) CHECK(l1[j][0] * DCOLS + l1[j][1] > l1[j - 1][0] * DCOLS + l1[j - 1][1]);
  }
  /* 'R' on the last column, 'D' on the last row, overlaps and junk are skipped; short strings are fine */
  dom_load(&b2, "......R" "......D" "R.....?" "D");
  dom_status(&b2, &st);
  CHECK(st.placed == 3);   /* (1,6) down, (2,0) right, (3,0) down; the 'R' in the last column overruns */
  dom_load(&b2, "");
  dom_status(&b2, &st);
  CHECK(st.placed == 0);
  {
    char big[DOM_SAVE_LEN + 1];
    memset(big, 'D', DCELLS); big[DCELLS] = 0;
    dom_load(&b2, big);
    dom_status(&b2, &st);
    CHECK(st.placed == 28 && st.covered == 56); /* column-wise stacking: pairs (0,c)-(1,c), then (2,c)-(3,c), ... */
    memset(big, 'R', DCELLS);
    dom_load(&b2, big);
    dom_status(&b2, &st);
    CHECK(st.placed == 24 && st.covered == 48); /* 3 per row of 7 cells */
  }

  { /* the walkthrough: the steps place the 28 dominoes of the printed solution, each exactly once, and the last board wins */
    const DomPuzzle *p = &DPUZZLES[0];
    DomBoard b;
    DomStatus st;
    uint8_t emph[DCELLS], seen[DDOMS];
    int step, j, n = 0, cnt, e;
    CHECK(p->tutorial && p->nsol == DDOMS);
    memset(seen, 0, sizeof seen);
    for (step = 0; step < DM_TUT_STEPS; step++) {
      const DmTutStep *t = &DM_TUT[step];
      CHECK(t->nrev <= 5);
      for (j = 0; j < t->nrev; j++) {
        CHECK(t->rev[j] >= 0 && t->rev[j] < DDOMS && !seen[(int)t->rev[j]]);
        seen[(int)t->rev[j]] = 1;
        n++;
      }
      dm_tut_board(p, step, &b, emph);
      dom_status(&b, &st);
      CHECK(st.placed == n && st.nconflicts == 0);   /* every domino of the step was accepted, none twice */
      for (e = 0, cnt = 0; e < DCELLS; e++) cnt += emph[e];
      CHECK(cnt == 2 * t->nrev);
      for (e = 0; e < DCELLS; e++) CHECK(!emph[e] || b.partner[e] >= 0);
    }
    CHECK(n == DDOMS);
    dm_tut_board(p, DM_TUT_STEPS - 1, &b, emph);
    CHECK(dom_wins(&b));
    dm_tut_board(p, 1, &b, emph);                      /* step 2: nothing placed yet */
    dom_status(&b, &st);
    CHECK(st.placed == 0);
  }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
