/* Host test for digit_core: gcc -o /tmp/t test_core.c digit_core.c && /tmp/t */
#include <stdio.h>
#include <string.h>

#include "digit_core.h"
#include "digit_data.h"
#include "digit_tutorial.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* count complete grids that win, searching row by row (rows must hold on their own) */
static const DPuzzle *sp;
static DGrid sg;
static int nsol;
static void search(int r) {
  int c, v, d[4];
  if (r == 4) { if (dgrid_wins(&sg)) nsol++; return; }
  for (d[0] = 0; d[0] < 10; d[0]++) {
    if (!glyph_admits(sp->glyph[r][0], d[0])) continue;
    for (d[1] = 0; d[1] < 10; d[1]++) {
      if (!glyph_admits(sp->glyph[r][1], d[1])) continue;
      for (d[2] = 0; d[2] < 10; d[2]++) {
        if (!glyph_admits(sp->glyph[r][2], d[2])) continue;
        v = deval_line(d, sp->rowops[r]);
        if (v < 0 || v > 9 || !glyph_admits(sp->glyph[r][3], v)) continue;
        d[3] = v;
        for (c = 0; c < 4; c++) sg.d[r][c] = (int8_t)d[c];
        search(r + 1);
      }
    }
  }
  for (c = 0; c < 4; c++) sg.d[r][c] = -1;
}

int main(void) {
  int i, r, c, k;
  int a[3];
  CHECK(DNUM_PUZZLES == 19 && DPUZZLES[0].tutorial && !DPUZZLES[1].tutorial);

  /* left-to-right evaluation, no precedence; negative, zero divisor, inexact division are invalid */
  a[0] = 1; a[1] = 2; a[2] = 3; CHECK(deval_line(a, "+*") == 9);
  a[0] = 2; a[1] = 3; a[2] = 4; CHECK(deval_line(a, "*+") == 10);
  a[0] = 2; a[1] = 3; a[2] = 1; CHECK(deval_line(a, "-+") == -1);
  a[0] = 5; a[1] = 3; a[2] = 2; CHECK(deval_line(a, "--") == 0);
  a[0] = 5; a[1] = 0; a[2] = 2; CHECK(deval_line(a, ":+") == -1);
  a[0] = 7; a[1] = 2; a[2] = 2; CHECK(deval_line(a, ":+") == -1);
  a[0] = 8; a[1] = 2; a[2] = 2; CHECK(deval_line(a, ":+") == 6);
  a[0] = 0; a[1] = 5; a[2] = 3; CHECK(deval_line(a, ":+") == 3);
  a[0] = 5; a[1] = 5; a[2] = 9; CHECK(deval_line(a, "-:") == 0);

  /* glyph sets */
  CHECK(glyph_admits(1, 0) && glyph_admits(1, 8) && glyph_admits(1, 9) && !glyph_admits(1, 1));
  CHECK(glyph_admits(2, 6) && !glyph_admits(2, 9));
  CHECK(glyph_admits(3, 1) && glyph_admits(3, 4) && glyph_admits(3, 7) && !glyph_admits(3, 0));
  CHECK(glyph_admits(4, 2) && glyph_admits(4, 3) && !glyph_admits(4, 5));
  CHECK(glyph_admits(5, 3) && glyph_admits(5, 5) && glyph_admits(5, 9) && !glyph_admits(5, 0));
  CHECK(!glyph_admits(0, 0) && !glyph_admits(6, 0) && !glyph_admits(1, 10) && !glyph_admits(1, -1));

  for (i = 0; i < DNUM_PUZZLES; i++) {
    const DPuzzle *p = &DPUZZLES[i];
    DGrid g, h;
    DMarks m;
    char buf[DGRID_SAVE_LEN + 1];

    dgrid_init(&g, p);
    CHECK(!dgrid_complete(&g) && !dgrid_wins(&g) && dgrid_violations(&g, &m) == 0);

    /* the printed answer satisfies the rules: admitted digits, all 8 equations */
    for (r = 0; r < 4; r++)
      for (c = 0; c < 4; c++) {
        CHECK(glyph_admits(p->glyph[r][c], p->answer[r][c]));
        dgrid_set(&g, r, c, p->answer[r][c]);
      }
    CHECK(dgrid_complete(&g) && dgrid_violations(&g, &m) == 0 && dgrid_wins(&g));

    /* an incomplete grid never wins and never reports a half-entered line */
    h = g;
    dgrid_clear(&h, 2, 2);
    CHECK(!dgrid_complete(&h) && !dgrid_wins(&h) && dgrid_violations(&h, &m) == 0);

    /* a digit its glyph does not admit is flagged at once, even in an otherwise empty grid */
    dgrid_init(&h, p);
    for (k = 0; k < 10; k++)
      if (!glyph_admits(p->glyph[0][0], k)) break;
    dgrid_set(&h, 0, 0, k);
    CHECK(dgrid_violations(&h, &m) == 1 && m.bad[0][0]);

    /* a wrong result breaks the row and the column through it (or the glyph) */
    h = g;
    dgrid_set(&h, 3, 3, (p->answer[3][3] + 1) % 10);
    CHECK(!dgrid_wins(&h) && dgrid_violations(&h, &m) > 0 && m.bad[3][3]);

    /* save / load round trip, bad characters ignored */
    dgrid_save(&g, buf);
    CHECK(strlen(buf) == DGRID_SAVE_LEN);
    dgrid_init(&h, p);
    dgrid_load(&h, buf);
    CHECK(memcmp(g.d, h.d, sizeof g.d) == 0 && dgrid_wins(&h));
    dgrid_load(&h, "12x4..a");
    CHECK(h.d[0][0] == 1 && h.d[0][1] == 2 && h.d[0][2] == -1 && h.d[0][3] == 4 && h.d[1][0] == -1);

    dgrid_reset(&g);
    CHECK(!dgrid_complete(&g) && g.d[0][0] == -1);

    /* the answer is one of the solutions found by exhaustive search; report how many there are */
    sp = p; dgrid_init(&sg, p); nsol = 0;
    search(0);
    CHECK(nsol >= 1);
    if (nsol != 1) printf("note: puzzle %s has %d solutions (any of them wins)\n", p->id, nsol);
  }
  { /* the walkthrough: every cell is revealed once, no step shows a broken rule, the last board is the printed answer */
    const DPuzzle *p = &DPUZZLES[0];
    DGrid g;
    DMarks m;
    uint8_t emph[4][4], seen[4][4];
    int st, j, r, c, n = 0;
    CHECK(p->tutorial);
    memset(seen, 0, sizeof seen);
    for (st = 0; st < DG_TUT_STEPS; st++) {
      const DgTutStep *t = &DG_TUT[st];
      CHECK(t->hrow >= -1 && t->hrow < 4 && t->hcol >= -1 && t->hcol < 4 && t->nrev <= 4);
      for (j = 0; j < t->nrev; j++) {
        r = t->rev[j][0]; c = t->rev[j][1];
        CHECK(r >= 0 && r < 4 && c >= 0 && c < 4 && !seen[r][c]);
        seen[r][c] = 1;
        n++;
      }
      dg_tut_board(p, st, &g, emph);
      CHECK(dgrid_violations(&g, &m) == 0);
    }
    CHECK(n == 16);
    dg_tut_board(p, DG_TUT_STEPS - 1, &g, emph);
    CHECK(dgrid_complete(&g) && dgrid_wins(&g));
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) CHECK(g.d[r][c] == p->answer[r][c]);
    dg_tut_board(p, 2, &g, emph);  /* step 3: row 2 emphasised, nothing revealed yet */
    CHECK(emph[1][0] && emph[1][3] && !emph[0][0] && !emph[2][1] && g.d[1][0] == -1);
    dg_tut_board(p, 4, &g, emph);  /* step 5: column 1 */
    CHECK(emph[0][0] && emph[3][0] && !emph[0][1] && g.d[0][0] == p->answer[0][0] && g.d[2][1] == -1);
  }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
