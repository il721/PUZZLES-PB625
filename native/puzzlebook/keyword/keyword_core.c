/* Keyword Rebuses rules; see keyword_core.h. */
#include <string.h>

#include "keyword_core.h"

static const char ROW_OPS[3] = {'*', '+', '+'};
static const char COL_OPS[3] = {'+', '*', '-'};

int kw_row_op(int r) { return ROW_OPS[r]; }
int kw_col_op(int c) { return COL_OPS[c]; }

void kw_reset(KwBoard *b) {
  memset(b->val, -1, sizeof b->val);
  b->sel = -1;
}

void kw_init(KwBoard *b, const KwPuzzle *p) {
  b->p = p;
  kw_reset(b);
}

int kw_set(KwBoard *b, int letter, int digit) {
  if (letter < 0 || letter >= KW_LETTERS || digit < -1 || digit > 9) return 0;
  b->val[letter] = (int8_t)digit;
  return 1;
}

int kw_word_value(const KwBoard *b, int row, int col) {
  int k, v = 0;
  for (k = 0; k < b->p->len[row][col]; k++) {
    int d = b->val[b->p->w[row][col][k]];
    if (d < 0) return -1;
    v = v * 10 + d;
  }
  return v;
}

static int apply(int op, int a, int c) { return op == '*' ? a * c : op == '+' ? a + c : a - c; }

void kw_status(const KwBoard *b, KwStatus *st) {
  int i, d, r, c, e;
  memset(st, 0, sizeof *st);
  for (d = 0; d < 10; d++) st->holder[d] = -1;
  for (i = 0; i < KW_LETTERS; i++) {
    if (b->val[i] < 0) continue;
    st->set++;
    st->holders[b->val[i]]++;
    st->holder[b->val[i]] = (int8_t)i;
  }
  for (d = 0; d < 10; d++)
    if (st->holders[d] != 1) st->holder[d] = -1;
  for (i = 0; i < KW_LETTERS; i++)
    if (b->val[i] >= 0 && st->holders[b->val[i]] > 1) { st->conflict[i] = 1; st->nconflict++; }
  for (r = 0; r < 3; r++)
    for (c = 0; c < 3; c++)
      if (b->p->len[r][c] >= 2 && b->val[b->p->w[r][c][0]] == 0) { st->lead0[r][c] = 1; st->nlead0++; }
  for (e = 0; e < 6; e++) {
    int a, x, z;
    if (e < 3) { a = kw_word_value(b, e, 0); x = kw_word_value(b, e, 1); z = kw_word_value(b, e, 2); }
    else { a = kw_word_value(b, 0, e - 3); x = kw_word_value(b, 1, e - 3); z = kw_word_value(b, 2, e - 3); }
    if (a < 0 || x < 0 || z < 0) { st->eq[e] = KW_INCOMPLETE; continue; }
    st->eq[e] = apply(e < 3 ? ROW_OPS[e] : COL_OPS[e - 3], a, x) == z ? KW_OK : KW_WRONG;
    if (st->eq[e] == KW_WRONG) st->nwrong++;
  }
  /* ten letters on ten different digits = a bijection onto 0..9 */
  st->solved = st->set == KW_LETTERS && st->nconflict == 0 && st->nlead0 == 0 && st->nwrong == 0;
}

int kw_wins(const KwBoard *b) {
  KwStatus st;
  kw_status(b, &st);
  return st.solved;
}

int kw_keyword(const KwBoard *b, uint16_t out[10]) {
  KwStatus st;
  int d;
  kw_status(b, &st);
  memset(out, 0, 10 * sizeof out[0]);
  if (st.set != KW_LETTERS || st.nconflict) return 0;
  for (d = 0; d < 10; d++) out[d] = b->p->let[st.holder[d]];
  return 1;
}

void kw_save(const KwBoard *b, char *out) {
  int i;
  for (i = 0; i < KW_LETTERS; i++) out[i] = b->val[i] < 0 ? '-' : (char)('0' + b->val[i]);
  out[KW_LETTERS] = 0;
}

void kw_load(KwBoard *b, const char *s) {
  int i;
  kw_reset(b);
  for (i = 0; i < KW_LETTERS; i++) {
    if (!s[i]) return;
    if (s[i] >= '0' && s[i] <= '9') b->val[i] = (int8_t)(s[i] - '0');
  }
}
