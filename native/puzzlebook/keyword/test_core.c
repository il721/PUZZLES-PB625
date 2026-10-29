/* Host test for keyword_core: gcc -O2 -o /tmp/t test_core.c keyword_core.c && /tmp/t */
#include <stdio.h>
#include <string.h>

#include "keyword_core.h"
#include "keyword_data.h"
#include "keyword_tutorial.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* exhaustive search: letters in order of first appearance, distinct digits, no leading zero, every equation tested the
 * moment its last letter is placed; counts solutions up to a cap and keeps the first */
static KwBoard sb;
static int nsol, cap;
static int8_t first[KW_LETTERS];
static int lead[KW_LETTERS];

static int lets_placed(const KwBoard *b, int r, int c, int e, int upto) {
  int k, m = 0;
  (void)b;
  (void)e;
  for (k = 0; k < sb.p->len[r][c]; k++) if (sb.p->w[r][c][k] > m) m = sb.p->w[r][c][k];
  return m <= upto;
}

static int eq_ok_upto(int e, int upto) {
  int r[3], c[3], k;
  for (k = 0; k < 3; k++) { r[k] = e < 3 ? e : k; c[k] = e < 3 ? k : e - 3; }
  for (k = 0; k < 3; k++) if (!lets_placed(&sb, r[k], c[k], e, upto)) return 1; /* not complete yet: nothing to test */
  {
    KwStatus st;
    kw_status(&sb, &st);
    return st.eq[e] != KW_WRONG;
  }
}

static void search(int i) {
  int d, e;
  if (nsol >= cap) return;
  if (i == KW_LETTERS) {
    if (nsol == 0) memcpy(first, sb.val, sizeof first);
    nsol++;
    return;
  }
  for (d = 0; d < 10; d++) {
    int used = 0, k;
    if (d == 0 && lead[i]) continue;
    for (k = 0; k < i; k++) if (sb.val[k] == d) used = 1;
    if (used) continue;
    sb.val[i] = (int8_t)d;
    for (e = 0; e < 6; e++) if (!eq_ok_upto(e, i)) break;
    if (e == 6) search(i + 1);
    sb.val[i] = -1;
    if (nsol >= cap) return;
  }
}

static int solve(const KwPuzzle *p, int c) {
  int r, col;
  kw_init(&sb, p);
  memset(lead, 0, sizeof lead);
  for (r = 0; r < 3; r++)
    for (col = 0; col < 3; col++) if (p->len[r][col] >= 2) lead[p->w[r][col][0]] = 1;
  nsol = 0; cap = c;
  search(0);
  return nsol;
}

static void play_book(KwBoard *b) {
  int i;
  for (i = 0; i < KW_LETTERS; i++) kw_set(b, i, b->p->ans[i]);
}

int main(void) {
  int i, k, r, c;
  CHECK(KNUM_PUZZLES == 18);
  CHECK(KPUZZLES[0].tutorial && !KPUZZLES[1].tutorial);
  CHECK(kw_row_op(0) == '*' && kw_row_op(1) == '+' && kw_row_op(2) == '+');
  CHECK(kw_col_op(0) == '+' && kw_col_op(1) == '*' && kw_col_op(2) == '-');

  for (i = 0; i < KNUM_PUZZLES; i++) {
    const KwPuzzle *p = &KPUZZLES[i];
    KwBoard b;
    KwStatus st;
    uint16_t kwd[10];
    int seen[KW_LETTERS] = {0}, digs[10] = {0}, j;

    /* data: ten distinct Cyrillic letters, each word 1..3 letters, every letter used, the book's digits a permutation */
    for (k = 0; k < KW_LETTERS; k++) {
      CHECK(p->let[k] >= 0x0410 && p->let[k] <= 0x042F);
      for (j = 0; j < k; j++) CHECK(p->let[j] != p->let[k]);
      CHECK(p->ans[k] <= 9);
      digs[p->ans[k]]++;
    }
    for (k = 0; k < 10; k++) CHECK(digs[k] == 1);
    for (r = 0; r < 3; r++)
      for (c = 0; c < 3; c++) {
        CHECK(p->len[r][c] >= 1 && p->len[r][c] <= 3);
        for (k = 0; k < p->len[r][c]; k++) { CHECK(p->w[r][c][k] < KW_LETTERS); seen[p->w[r][c][k]] = 1; }
      }
    for (k = 0; k < KW_LETTERS; k++) CHECK(seen[k]);
    /* the bank order is the order of first appearance, so letter k is first used at or before position k */
    { int next = 0; for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) for (k = 0; k < p->len[r][c]; k++) { if (p->w[r][c][k] == next) next++; else CHECK(p->w[r][c][k] < next); } CHECK(next == KW_LETTERS); }

    /* a fresh board: nothing set, nothing flagged, not solved */
    kw_init(&b, p);
    kw_status(&b, &st);
    CHECK(st.set == 0 && st.nconflict == 0 && st.nlead0 == 0 && st.nwrong == 0 && !st.solved && !kw_wins(&b));
    for (k = 0; k < 6; k++) CHECK(st.eq[k] == KW_INCOMPLETE);
    CHECK(!kw_keyword(&b, kwd));

    /* the book's answer wins, every equation is ok, the keyword comes out in digit order */
    play_book(&b);
    kw_status(&b, &st);
    CHECK(st.solved && kw_wins(&b) && st.set == 10);
    for (k = 0; k < 6; k++) CHECK(st.eq[k] == KW_OK);
    CHECK(kw_keyword(&b, kwd));
    for (k = 0; k < 10; k++) CHECK(st.holder[k] >= 0 && p->ans[st.holder[k]] == k && kwd[k] == p->let[st.holder[k]]);

    /* every single wrong digit on a letter of the book's answer breaks the win (give it any other digit) */
    for (k = 0; k < KW_LETTERS; k++)
      for (j = 0; j < 10; j++) {
        if (j == p->ans[k]) continue;
        play_book(&b);
        kw_set(&b, k, j);
        CHECK(!kw_wins(&b));
        kw_status(&b, &st);
        CHECK(st.nconflict == 2 || (j == 0 && st.nlead0 >= 0)); /* swapping onto another letter's digit flags both */
      }
    /* removing any letter's digit un-solves and leaves its equations incomplete */
    for (k = 0; k < KW_LETTERS; k++) {
      play_book(&b);
      CHECK(kw_set(&b, k, -1));
      CHECK(!kw_wins(&b));
      kw_status(&b, &st);
      CHECK(st.set == 9 && st.nconflict == 0);
    }

    /* the search finds exactly one solution, and it is the book's */
    CHECK(solve(p, 2) == 1);
    for (k = 0; k < KW_LETTERS; k++) CHECK(first[k] == p->ans[k]);

    /* save / load round trip */
    play_book(&b);
    kw_set(&b, 3, -1);
    { char buf[KW_SAVE_LEN + 1]; KwBoard b2; kw_save(&b, buf); CHECK((int)strlen(buf) == KW_SAVE_LEN); kw_init(&b2, p); kw_load(&b2, buf); CHECK(!memcmp(b.val, b2.val, sizeof b.val)); }
  }

  /* hand-made rules on puzzle p01 / the sample */
  {
    const KwPuzzle *p = &KPUZZLES[0]; /* ex1: ПЗ x А = ПЕП ... letters П З А Е У Т И Г Н О */
    KwBoard b;
    KwStatus st;
    uint16_t kwd[10];
    int i2;
    kw_init(&b, p);
    CHECK(kw_set(&b, 0, 2) && kw_set(&b, 1, 8));     /* П = 2, З = 8 -> ПЗ = 28 */
    CHECK(kw_word_value(&b, 0, 0) == 28 && kw_word_value(&b, 0, 1) == -1);
    CHECK(!kw_set(&b, 10, 1) && !kw_set(&b, -1, 1) && !kw_set(&b, 0, 10) && !kw_set(&b, 0, -2)); /* bad arguments change nothing */
    CHECK(b.val[0] == 2);
    /* two letters on one digit: both flagged, never refused, and it cannot win */
    kw_set(&b, 1, 2);
    kw_status(&b, &st);
    CHECK(st.nconflict == 2 && st.conflict[0] && st.conflict[1] && st.holders[2] == 2 && st.holder[2] == -1);
    kw_set(&b, 1, 8);
    kw_status(&b, &st);
    CHECK(st.nconflict == 0 && st.holder[2] == 0 && st.holder[8] == 1 && st.holder[5] == -1);
    /* leading zero: ПЗ cannot start with 0, the single-letter А may be 0 */
    kw_set(&b, 0, 0);
    kw_status(&b, &st);
    CHECK(st.nlead0 >= 1 && st.lead0[0][0] && !st.solved);
    kw_reset(&b);
    kw_set(&b, 2, 0);                                 /* А = 0: a one-letter word, fine */
    kw_status(&b, &st);
    CHECK(st.nlead0 == 0 && !st.lead0[0][1]);
    /* a failing equation shows only once all its letters have digits; the partial ones stay neutral */
    kw_reset(&b);
    kw_set(&b, 0, 2); kw_set(&b, 1, 8); kw_set(&b, 2, 9); kw_set(&b, 3, 5);  /* П З А Е = 2 8 9 5 */
    kw_status(&b, &st);
    CHECK(st.eq[0] == KW_OK);                          /* ПЗ x А = ПЕП: 28 x 9 = 252 */
    kw_set(&b, 3, 6);
    kw_status(&b, &st);
    CHECK(st.eq[0] == KW_WRONG && st.nwrong == 1 && st.eq[1] == KW_INCOMPLETE);
    /* the keyword of the sample */
    play_book(&b);
    CHECK(kw_keyword(&b, kwd));
    { const uint16_t want[10] = {0x0413, 0x0418, 0x041F, 0x041E, 0x0422, 0x0415, 0x041D, 0x0423, 0x0417, 0x0410}; /* ГИПОТЕНУЗА */
      for (i2 = 0; i2 < 10; i2++) CHECK(kwd[i2] == want[i2]); }
    /* the same letters on a wrong bijection give a keyword but no win */
    kw_set(&b, 0, 3); kw_set(&b, 9, 2);               /* swap the digits of П (2) and О (3): still a bijection, equations break */
    CHECK(!kw_wins(&b) && kw_keyword(&b, kwd));
    /* tolerant load: junk and short strings */
    kw_load(&b, "x");
    CHECK(b.val[0] == -1);
    kw_load(&b, "01-?3");
    CHECK(b.val[0] == 0 && b.val[1] == 1 && b.val[2] == -1 && b.val[3] == -1 && b.val[4] == 3 && b.val[5] == -1);
    kw_load(&b, "");
    CHECK(b.val[0] == -1 && b.sel == -1);
  }

  { /* the walkthrough: the ten letters get their digits once each, no step shows a conflict or a broken equation, the last
     * board is solved and spells the book's keyword ГИПОТЕНУЗА */
    const KwPuzzle *p = &KPUZZLES[0];
    KwBoard b;
    KwStatus st;
    uint8_t emph[3][3];
    uint8_t seen[KW_LETTERS];
    uint16_t word[10];
    static const uint16_t KEYWORD[10] = {0x413, 0x418, 0x41F, 0x41E, 0x422, 0x415, 0x41D, 0x423, 0x417, 0x410};
    int step, j, n = 0, l, r, c, cnt;
    CHECK(p->tutorial);
    memset(seen, 0, sizeof seen);
    for (step = 0; step < KT_TUT_STEPS; step++) {
      const KtTutStep *t = &KT_TUT[step];
      CHECK(t->nrev <= 3 && t->nword <= 5);
      for (j = 0; j < t->nrev; j++) {
        l = kt_tut_letter(p, t->rev[j]);
        CHECK(l >= 0 && !seen[l]);
        if (l >= 0) seen[l] = 1;
        n++;
      }
      for (j = 0; j < t->nword; j++) CHECK(t->word[j][0] >= 0 && t->word[j][0] < 3 && t->word[j][1] >= 0 && t->word[j][1] < 3);
      kt_tut_board(p, step, &b, emph);
      kw_status(&b, &st);
      CHECK(st.nconflict == 0 && st.nlead0 == 0 && st.nwrong == 0 && st.set == n);
      for (r = 0, cnt = 0; r < 3; r++) for (c = 0; c < 3; c++) cnt += emph[r][c];
      CHECK(cnt == t->nword);
    }
    CHECK(n == KW_LETTERS);
    kt_tut_board(p, KT_TUT_STEPS - 1, &b, emph);
    CHECK(kw_wins(&b) && kw_keyword(&b, word) == 1);
    for (j = 0; j < 10; j++) CHECK(word[j] == KEYWORD[j]);
    kt_tut_board(p, 1, &b, emph);   /* step 2: И = 1 only, column 1 emphasised */
    CHECK(b.val[kt_tut_letter(p, K_I)] == 1 && emph[0][0] && emph[1][0] && emph[2][0] && !emph[0][1]);
  }

  printf("keyword_core: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
