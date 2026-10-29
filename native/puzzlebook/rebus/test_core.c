/* Host test for rebus_core: gcc -o /tmp/t test_core.c rebus_core.c && /tmp/t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rebus_core.h"
#include "rebus_data.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void fill_canonical(Grid *g) {
  int r, s, k;
  for (r = 0; r < 5; r++)
    for (s = 0; s < 5; s++) {
      const char *n = puzzle_number(g->p, r, s);
      for (k = 0; k < g->width[r][s]; k++) grid_set(g, r, s, k, n[k] - '0');
    }
}

int main(void) {
  int i, r, s, k;
  for (i = 0; i < NUM_PUZZLES; i++) {
    const Puzzle *p = &PUZZLES[i];
    Grid g, h;
    Marks m;
    char buf[GRID_SAVE_LEN + 1];
    long nums[4], res;
    int total = 0, resum = 0, j;

    grid_init(&g, p);
    CHECK(!grid_complete(&g));
    CHECK(!grid_wins(&g));
    CHECK(grid_violations(&g, &m) == 0); /* empty grid: nothing fires */

    /* data check, independent of the grid code: rows, columns, total */
    for (r = 0; r < 4; r++) {
      for (s = 0; s < 4; s++) nums[s] = atol(p->rows[r].nums[s]);
      CHECK(eval_row(nums, p->rows[r].ops, &res) == 0 && res == atol(p->rows[r].result));
      resum += (int)res;
    }
    for (j = 0; j < 4; j++) {
      int sum = 0;
      for (r = 0; r < 4; r++) sum += atoi(p->rows[r].nums[j]);
      CHECK(sum == atoi(p->rows[j].result));
      total += atoi(p->rows[j].result);
    }
    CHECK(total == atoi(p->total) && resum == total);

    /* givens match canonical digits and are locked */
    for (k = 0; k < p->ngivens; k++) {
      const int *e = p->givens[k];
      CHECK(puzzle_number(p, e[0], e[1])[e[2]] - '0' == e[3]);
      CHECK(g.given[e[0]][e[1]][e[2]] && !grid_set(&g, e[0], e[1], e[2], (e[3] + 1) % 10));
    }

    fill_canonical(&g);
    CHECK(grid_complete(&g));
    CHECK(grid_violations(&g, &m) == 0 && grid_wins(&g));

    /* save / load round trip: only editable cells come back */
    grid_save(&g, buf);
    grid_init(&h, p);
    grid_load(&h, buf);
    CHECK(grid_wins(&h));
    grid_reset(&h);
    CHECK(!grid_complete(&h) && h.given[p->givens[0][0]][p->givens[0][1]][p->givens[0][2]]);

    /* every single wrong digit in an editable cell breaks a rule */
    for (r = 0; r < 5; r++)
      for (s = 0; s < 5; s++)
        for (k = 0; k < g.width[r][s]; k++) {
          int old = g.digit[r][s][k];
          if (g.given[r][s][k]) continue;
          grid_set(&g, r, s, k, (old + 1) % 10);
          CHECK(!grid_wins(&g));
          grid_set(&g, r, s, k, old);
        }
    CHECK(grid_wins(&g));
  }

  { /* leading zero / zero are flagged */
    Grid g;
    Marks m;
    grid_init(&g, &PUZZLES[1]);
    grid_set(&g, 0, 0, 0, 0);
    grid_set(&g, 0, 0, 1, 7);
    CHECK(grid_violations(&g, &m) >= 1 && m.bad[0][0][0]);
  }
  { /* a wrong result is flagged only once its number is complete */
    Grid g;
    Marks m;
    grid_init(&g, &PUZZLES[1]);
    fill_canonical(&g);
    grid_set(&g, 0, 4, 1, 9); /* 50 -> 59 */
    CHECK(grid_violations(&g, &m) >= 1 && m.bad[0][4][0] && m.bad[0][0][0]);
    grid_clear(&g, 0, 4, 1);
    CHECK(grid_violations(&g, &m) == 0);
  }
  { /* division by zero / inexact is invalid */
    long n1[4] = {7, 2, 1, 1}, n2[4] = {8, 0, 1, 1}, o;
    CHECK(eval_row(n1, ":+*", &o) == -1);
    CHECK(eval_row(n2, ":+*", &o) == -1);
    CHECK(eval_row(n1, "+:*", &o) == -1 || o == 9);
  }

  printf("%d checks, %d failed\n", checks, fails);
  return fails != 0;
}
