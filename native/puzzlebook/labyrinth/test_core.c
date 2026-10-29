/* Host test for labyrinth_core: gcc -o /tmp/t test_core.c labyrinth_core.c && /tmp/t */
#include <stdio.h>
#include <string.h>

#include "labyrinth_core.h"
#include "labyrinth_data.h"
#include "labyrinth_tutorial.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* exhaustive solver (the source's DFS): self-avoiding path of 33 cells from cell 0 to cell 63, every letter once */
static const LabPuzzle *sp;
static int vis[LCELLS], nsol, cap, cur[LPATH], first[LPATH];
static long nodes;
static unsigned long long used;
static int search(int depth, int cell) { /* depth = cells on the path so far; returns 0 to stop */
  static const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
  int k;
  nodes++;
  if (depth == LPATH) {
    if (cell == LEND) { if (nsol == 0) memcpy(first, cur, sizeof first); nsol++; return nsol < cap; }
    return 1;
  }
  for (k = 0; k < 4; k++) {
    int r = cell / 8 + dr[k], c = cell % 8 + dc[k], n, rem, d;
    if (r < 0 || r > 7 || c < 0 || c > 7) continue;
    n = r * 8 + c;
    if (vis[n] || (used >> sp->letter[n] & 1)) continue;
    if (n == LEND && depth + 1 != LPATH) continue;
    rem = LPATH - depth;
    d = (7 - r) + (7 - c);
    if (d > rem - 1 || ((rem - 1 - d) & 1)) continue;
    vis[n] = 1; used |= 1ULL << sp->letter[n]; cur[depth] = n;
    if (!search(depth + 1, n)) return 0;
    vis[n] = 0; used &= ~(1ULL << sp->letter[n]);
  }
  return 1;
}
static int solve(const LabPuzzle *p, int c) {
  sp = p; cap = c; nsol = 0; nodes = 0;
  memset(vis, 0, sizeof vis);
  vis[0] = 1; used = 1ULL << p->letter[0]; cur[0] = 0;
  search(1, 0);
  return nsol;
}

static int adj(int a, int b) {
  int dr = a / 8 - b / 8, dc = a % 8 - b % 8;
  return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc) == 1;
}

int main(void) {
  int i, k, j;
  LabBoard b, b2;
  LabStatus st;
  const LabPuzzle *ex = &LPUZZLES[0];

  CHECK(LNUM_PUZZLES == 19 && ex->tutorial && !LPUZZLES[1].tutorial);

  /* data: anchors А (0) and Я (32); every letter occurs once or twice; recorded paths are valid */
  for (i = 0; i < LNUM_PUZZLES; i++) {
    const LabPuzzle *p = &LPUZZLES[i];
    int cnt[33] = {0}, seen[33] = {0};
    CHECK(p->letter[LSTART] == 0 && p->letter[LEND] == 32);
    for (k = 0; k < LCELLS; k++) cnt[p->letter[k]]++;
    for (k = 0; k < 33; k++) CHECK(cnt[k] >= 1 && cnt[k] <= 3);
    CHECK(p->nsol == 33);
    CHECK(p->sol[0] == 0 && p->sol[32] == 63);
    for (k = 0; k < 33; k++) {
      CHECK(!seen[p->letter[p->sol[k]]]++);
      if (k) CHECK(adj(p->sol[k - 1], p->sol[k]));
    }
  }

  /* playing the recorded path from both ends (the first m cells from A, the rest backwards from Z) wins on the last
   * tap; some split works for every puzzle (a path that touches the far anchor early would be "joined" too soon) */
  for (i = 0; i < LNUM_PUZZLES; i++) {
    const LabPuzzle *p = &LPUZZLES[i];
    int m, ok = 0;
    for (m = 1; m <= 31 && !ok; m++) {
      int good = 1;
      lab_init(&b, p);
      for (k = 1; k <= m && good; k++) good = lab_tap(&b, p->sol[k]);
      for (k = 31; k > m && good; k--) {
        good = !lab_wins(&b) && !lab_joined(&b) && lab_tap(&b, p->sol[k]);
      }
      if (good && lab_wins(&b)) {
        lab_status(&b, &st);
        CHECK(st.joined && st.placed == 33 && st.solved && b.na + b.nz == 33);
        ok = 1;
      }
    }
    CHECK(ok);
  }

  /* exhaustive search: every puzzle has exactly one path, and it is the recorded one */
  for (i = 0; i < LNUM_PUZZLES; i++) {
    int n = solve(&LPUZZLES[i], 2);
    CHECK(n == 1);
    if (n != 1) printf("note: %s has %d solutions\n", LPUZZLES[i].id, n);
    for (k = 0; k < 33; k++) CHECK(first[k] == LPUZZLES[i].sol[k]);
  }

  /* tap rules on the sample: row 0 = АОДТЧЗУА, row 1 = РИЩШЙПКЮ; cell 8 = Р below the start */
  lab_init(&b, ex);
  CHECK(!lab_tap(&b, 0) && b.na == 1);                  /* the anchor of a one-cell chain does nothing */
  CHECK(!lab_tap(&b, 63) && b.nz == 1);
  CHECK(lab_tap(&b, 8) && b.na == 2 && b.a[1] == 8);    /* extend A */
  CHECK(lab_tap(&b, 8) && b.na == 1);                   /* tapping the head retracts it */
  CHECK(lab_tap(&b, 62) && b.nz == 2 && b.z[1] == 62);  /* extend Z */
  CHECK(lab_tap(&b, 62) && b.nz == 1);
  CHECK(lab_tap(&b, 27) && b.mark[27] && !b.cross[27]); /* not adjacent to a head: a mark */
  CHECK(lab_tap(&b, 27) && !b.mark[27]);                /* toggles */
  CHECK(!lab_tap(&b, -1) && !lab_tap(&b, 64));
  CHECK(lab_cross(&b, 27) && b.cross[27]);              /* long press: manual cross */
  CHECK(lab_tap(&b, 27) && b.mark[27] && !b.cross[27]); /* a mark clears the cross */
  CHECK(lab_cross(&b, 27) && b.cross[27] && !b.mark[27]);
  CHECK(lab_cross(&b, 27) && !b.cross[27]);
  CHECK(lab_tap(&b, 1) && b.na == 2);                   /* extend A to the right instead; clears nothing on the cell */
  /* extending clears an annotation on the cell */
  lab_init(&b, ex);
  lab_cross(&b, 8); CHECK(b.cross[8]);
  CHECK(lab_tap(&b, 8) && !b.cross[8] && b.na == 2);
  lab_tap(&b, 8);                                       /* retract: the cross is not restored */
  CHECK(!b.cross[8] && !b.mark[8]);
  /* a cell adjacent to both heads goes to chain A (tie): put the heads two apart with one free cell between */
  lab_init(&b, ex);
  {
    /* sample solution: ... 4,3 (cell 35?) uses recorded cells: A takes sol[1..], Z takes sol[31..] */
    for (k = 1; k <= 30; k++) lab_tap(&b, ex->sol[k]);      /* A reaches sol[30] */
    CHECK(b.na == 31 && !lab_joined(&b));                   /* sol[30] and the anchor 63 are two apart */
    CHECK(lab_tap(&b, ex->sol[31]) && b.na == 32 && b.nz == 1); /* adjacent to both heads: A wins the tie */
    CHECK(lab_joined(&b) && lab_wins(&b));
  }
  /* join is terminal: no extension, no marks; retraction still works */
  CHECK(!lab_tap(&b, 5) && !b.mark[5]);
  CHECK(lab_tap(&b, ex->sol[31]) && b.na == 31 && !lab_joined(&b)); /* head retracts */
  /* path length cap: never more than 33 cells in total */
  lab_init(&b, ex);
  for (k = 1; k <= 31; k++) lab_tap(&b, ex->sol[k]);
  CHECK(b.na + b.nz == 33 || b.na + b.nz == 32);
  /* long press cuts a chain back to just before the cell; anchors are never cut */
  lab_init(&b, ex);
  for (k = 1; k <= 10; k++) lab_tap(&b, ex->sol[k]);
  CHECK(!lab_cross(&b, 0));
  CHECK(lab_cross(&b, ex->sol[4]) && b.na == 4 && b.a[3] == ex->sol[3]);
  CHECK(!lab_cross(&b, 63) && b.nz == 1);
  lab_tap(&b, 62); lab_tap(&b, 61);
  CHECK(b.nz == 3 && lab_cross(&b, 62) && b.nz == 1);

  /* status: duplicates, missing, auto-crosses */
  lab_init(&b, ex);
  lab_status(&b, &st);
  /* Ex: А occurs at cells 0 and 7 (and Я at 7 'А'? row 0 ends with А): the twin of the start is crossed automatically */
  CHECK(ex->letter[7] == 0 && st.auto_cross[7] && !st.auto_cross[0]);
  CHECK(st.used[0] == 1 && st.used[32] == 1 && st.ndup == 0 && st.nmissing == 31);
  /* a letter used twice on the path is a duplicate and blocks the win even with 33 cells */
  lab_init(&b, ex);
  for (k = 1; k <= 7; k++) CHECK(lab_tap(&b, k));     /* along the top row: А О Д Т Ч З У А */
  lab_status(&b, &st);
  CHECK(st.used[0] == 2 && st.ndup == 1 && !st.solved);

  /* retraction clears auto-crosses (they are derived) */
  lab_init(&b, ex);
  lab_status(&b, &st); CHECK(st.auto_cross[7]);
  lab_cross(&b, 7); CHECK(b.cross[7]);

  /* save / load round trip, tolerant load */
  lab_init(&b, &LPUZZLES[3]);
  for (k = 1; k <= 12; k++) lab_tap(&b, LPUZZLES[3].sol[k]);
  for (k = 31; k >= 28; k--) lab_tap(&b, LPUZZLES[3].sol[k]);
  lab_cross(&b, 13 + 0); lab_tap(&b, 40); lab_tap(&b, 41);
  {
    char s[LAB_SAVE_LEN], *f[4];
    char copy[LAB_SAVE_LEN];
    lab_save(&b, s);
    strcpy(copy, s);
    f[0] = strtok(copy, " "); for (j = 1; j < 4; j++) f[j] = strtok(NULL, " ");
    CHECK(f[0] && f[1] && f[2] && f[3] && strlen(f[0]) == (size_t)b.na && strlen(f[1]) == (size_t)b.nz);
    lab_init(&b2, &LPUZZLES[3]);
    CHECK(lab_load(&b2, f[0], f[1], f[2], f[3]));
    CHECK(b2.na == b.na && b2.nz == b.nz && !memcmp(b.a, b2.a, (size_t)b.na) && !memcmp(b.z, b2.z, (size_t)b.nz));
    CHECK(!memcmp(b.cross, b2.cross, sizeof b.cross) && !memcmp(b.mark, b2.mark, sizeof b.mark));
    /* empty lists are written as "-" */
    lab_init(&b, &LPUZZLES[3]);
    lab_save(&b, s);
    CHECK(!strcmp(s, "0 o - -"));
    CHECK(lab_load(&b2, "0", "o", "-", "-") && b2.na == 1 && b2.nz == 1);
  }
  /* bad saves leave a fresh board: wrong anchor, jump, overlap, too long, junk, out of range */
  lab_init(&b2, &LPUZZLES[3]);
  CHECK(!lab_load(&b2, "1", "o", "-", "-") && b2.na == 1 && b2.a[0] == 0);
  CHECK(!lab_load(&b2, "0", "n", "-", "-"));
  CHECK(!lab_load(&b2, "02", "o", "-", "-"));            /* not adjacent */
  CHECK(!lab_load(&b2, "018", "o", "-", "-"));           /* 0 -> 1 -> 8: not adjacent */
  CHECK(!lab_load(&b2, "010", "o", "-", "-"));           /* repeated cell */
  CHECK(!lab_load(&b2, "0", "o", "p", "-"));             /* cross out of range ('p' = 64) */
  CHECK(!lab_load(&b2, "0", "o", "-", "\x7f"));
  CHECK(!lab_load(&b2, "0", "o", "-", "-x"));
  CHECK(!lab_load(&b2, "-", "o", "-", "-"));
  CHECK(!lab_load(&b2, "0", "-", "-", "-"));
  {
    char longa[40], longz[40];
    /* chain A and chain Z each valid but together longer than 33 */
    for (k = 0; k < 20; k++) longa[k] = (char)('0' + LPUZZLES[1].sol[k]);
    longa[20] = 0;
    for (k = 0; k < 14; k++) longz[k] = (char)('0' + LPUZZLES[1].sol[32 - k]);
    longz[14] = 0;
    CHECK(!lab_load(&b2, longa, longz, "-", "-"));       /* 20 + 14 = 34 */
    longz[13] = 0;
    lab_init(&b2, &LPUZZLES[1]);
    CHECK(lab_load(&b2, longa, longz, "-", "-") && lab_joined(&b2) && lab_wins(&b2)); /* 20 + 13 = 33: solved */
  }
  /* a cell shared by both chains */
  {
    uint8_t a[3] = {0, 1, 2}, z[3] = {63, 62, 2};
    CHECK(!lab_restore(&b2, a, 3, z, 3, NULL, 0, NULL, 0));
    z[2] = 61;
    CHECK(lab_restore(&b2, a, 3, z, 3, NULL, 0, NULL, 0) && b2.na == 3 && b2.nz == 3);
  }

  { /* the walkthrough: the nine chunks cover the printed path 0..32 once, step 3 shows twins crossed and a unique letter marked */
    const LabPuzzle *p = &LPUZZLES[0];
    LabBoard b;
    LabStatus st;
    uint8_t emph[LCELLS];
    int step, i, next = 0, cnt, occurrences;
    CHECK(p->tutorial && p->nsol == LPATH);
    for (step = 0; step < LB_TUT_STEPS; step++) {
      const LbTutStep *t = &LB_TUT[step];
      if (t->rlo >= 0) {
        CHECK(t->rlo == next && t->rhi >= t->rlo && t->rhi < LPATH);   /* contiguous, ascending, no gaps or repeats */
        next = t->rhi + 1;
      }
      CHECK(t->hlo < 0 || (t->hlo >= 0 && t->hhi >= t->hlo && t->hhi < LPATH));
      lb_tut_board(p, step, &b, emph);
      lab_status(&b, &st);
      CHECK(st.ndup == 0);
      cnt = 0;
      for (i = 0; i < LCELLS; i++) cnt += emph[i];
      CHECK(cnt == (t->hlo < 0 ? 0 : t->hhi - t->hlo + 1));
    }
    CHECK(next == LPATH);
    lb_tut_board(p, LB_TUT_STEPS - 1, &b, emph);
    CHECK(lab_wins(&b));
    {
      int8_t path[LPATH];
      CHECK(lab_path(&b, path) == LPATH);
      for (i = 0; i < LPATH; i++) CHECK(path[i] == p->sol[i]);
    }
    lb_tut_board(p, 2, &b, emph);   /* step 3 */
    CHECK(b.na == 1 && b.nz == 1);
    CHECK(b.cross[LB_TUT[2].cross[0]] && b.cross[LB_TUT[2].cross[1]] && b.mark[LB_TUT[2].mark[0]]);
    CHECK(p->letter[LB_TUT[2].cross[0]] == p->letter[LB_TUT[2].cross[1]]);   /* the twin pair */
    for (i = 0, occurrences = 0; i < LCELLS; i++) occurrences += p->letter[i] == p->letter[LB_TUT[2].mark[0]];
    CHECK(occurrences == 1);                                              /* the marked letter occurs only once */
    lb_tut_board(p, 3, &b, emph);   /* step 4: cells 0..3, no crosses or marks left over */
    CHECK(b.na == 4 && b.nz == 1 && b.cross[LB_TUT[2].cross[0]] == 0 && b.mark[LB_TUT[2].mark[0]] == 0);
    CHECK(emph[p->sol[0]] && emph[p->sol[3]] && !emph[p->sol[4]]);
    lb_tut_board(p, 0, &b, emph);
    CHECK(emph[LSTART] == 1);
    lb_tut_board(p, 1, &b, emph);
    CHECK(emph[LEND] == 1);
  }
  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
