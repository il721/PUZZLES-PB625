#include <string.h>

#include "labyrinth_core.h"

static int row(int c) { return c / 8; }
static int col(int c) { return c % 8; }
static int dist(int a, int b) {
  int dr = row(a) - row(b), dc = col(a) - col(b);
  return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
}

void lab_reset(LabBoard *b) {
  b->a[0] = LSTART; b->na = 1;
  b->z[0] = LEND; b->nz = 1;
  memset(b->cross, 0, sizeof b->cross);
  memset(b->mark, 0, sizeof b->mark);
}

void lab_init(LabBoard *b, const LabPuzzle *p) {
  b->p = p;
  lab_reset(b);
}

int lab_on_path(const LabBoard *b, int cell) {
  int i;
  for (i = 0; i < b->na; i++) if (b->a[i] == cell) return 1;
  for (i = 0; i < b->nz; i++) if (b->z[i] == cell) return 1;
  return 0;
}

int lab_joined(const LabBoard *b) { return dist(b->a[b->na - 1], b->z[b->nz - 1]) == 1; }

int lab_tap(LabBoard *b, int c) {
  int ha = b->a[b->na - 1], hz = b->z[b->nz - 1], can;
  if (c < 0 || c >= LCELLS) return 0;
  if (c == ha) { if (b->na > 1) { b->na--; return 1; } return 0; }
  if (c == hz) { if (b->nz > 1) { b->nz--; return 1; } return 0; }
  if (lab_joined(b) || lab_on_path(b, c)) return 0;
  can = b->na + b->nz < LPATH;
  if (can && dist(c, ha) == 1) { b->a[b->na++] = (int8_t)c; b->cross[c] = b->mark[c] = 0; return 1; }
  if (can && dist(c, hz) == 1) { b->z[b->nz++] = (int8_t)c; b->cross[c] = b->mark[c] = 0; return 1; }
  if (b->mark[c]) b->mark[c] = 0;
  else { b->mark[c] = 1; b->cross[c] = 0; }
  return 1;
}

int lab_cross(LabBoard *b, int c) {
  int i;
  if (c < 0 || c >= LCELLS) return 0;
  for (i = 0; i < b->na; i++)
    if (b->a[i] == c) { if (i == 0) return 0; b->na = i; return 1; }
  for (i = 0; i < b->nz; i++)
    if (b->z[i] == c) { if (i == 0) return 0; b->nz = i; return 1; }
  if (b->cross[c]) b->cross[c] = 0;
  else { b->cross[c] = 1; b->mark[c] = 0; }
  return 1;
}

int lab_path(const LabBoard *b, int8_t out[LPATH]) {
  int i, n = 0;
  for (i = 0; i < b->na; i++) out[n++] = b->a[i];
  for (i = b->nz - 1; i >= 0; i--) out[n++] = b->z[i];
  return n;
}

void lab_status(const LabBoard *b, LabStatus *st) {
  int i, c;
  int8_t path[LPATH];
  int n = lab_path(b, path);
  memset(st, 0, sizeof *st);
  for (i = 0; i < n; i++) st->used[b->p->letter[path[i]]]++;
  for (i = 0; i < 33; i++) {
    if (st->used[i] > 1) st->ndup++;
    if (st->used[i] == 0) st->nmissing++;
  }
  st->joined = lab_joined(b);
  st->placed = n;
  for (c = 0; c < LCELLS; c++)
    if (!lab_on_path(b, c) && st->used[b->p->letter[c]] > 0) st->auto_cross[c] = 1;
  st->solved = st->joined && n == LPATH && st->ndup == 0 && st->nmissing == 0;
}

int lab_wins(const LabBoard *b) {
  LabStatus st;
  lab_status(b, &st);
  return st.solved;
}

static int valid_chain(const uint8_t *c, int n, int anchor) {
  int i, j;
  if (n < 1 || c[0] != anchor) return 0;
  for (i = 0; i < n; i++) {
    if (c[i] >= LCELLS) return 0;
    if (i && dist(c[i - 1], c[i]) != 1) return 0;
    for (j = 0; j < i; j++) if (c[j] == c[i]) return 0;
  }
  return 1;
}

int lab_restore(LabBoard *b, const uint8_t *a, int na, const uint8_t *z, int nz, const uint8_t *x, int nx, const uint8_t *m, int nm) {
  int i, j;
  lab_reset(b);
  if (na > LPATH || nz > LPATH || na + nz > LPATH) return 0;
  if (!valid_chain(a, na, LSTART) || !valid_chain(z, nz, LEND)) return 0;
  for (i = 0; i < na; i++) for (j = 0; j < nz; j++) if (a[i] == z[j]) return 0;
  for (i = 0; i < nx; i++) if (x[i] >= LCELLS) return 0;
  for (i = 0; i < nm; i++) if (m[i] >= LCELLS) return 0;
  for (i = 0; i < na; i++) b->a[i] = (int8_t)a[i];
  for (i = 0; i < nz; i++) b->z[i] = (int8_t)z[i];
  b->na = na; b->nz = nz;
  for (i = 0; i < nx; i++) b->cross[x[i]] = 1;
  for (i = 0; i < nm; i++) b->mark[m[i]] = 1;
  return 1;
}

static int emit(char *out, int n, const uint8_t *cells, int count) {
  int i;
  if (count == 0) out[n++] = '-';
  for (i = 0; i < count; i++) out[n++] = (char)('0' + cells[i]);
  out[n++] = ' ';
  return n;
}

void lab_save(const LabBoard *b, char *out) {
  uint8_t t[LCELLS];
  int i, n = 0, k;
  for (i = 0; i < b->na; i++) t[i] = (uint8_t)b->a[i];
  n = emit(out, n, t, b->na);
  for (i = 0; i < b->nz; i++) t[i] = (uint8_t)b->z[i];
  n = emit(out, n, t, b->nz);
  for (i = 0, k = 0; i < LCELLS; i++) if (b->cross[i]) t[k++] = (uint8_t)i;
  n = emit(out, n, t, k);
  for (i = 0, k = 0; i < LCELLS; i++) if (b->mark[i]) t[k++] = (uint8_t)i;
  n = emit(out, n, t, k);
  out[n - 1] = 0; /* no trailing space */
}

static int parse_field(const char *s, uint8_t *out, int max) {
  int n = 0;
  if (s[0] == '-' && (s[1] == 0 || s[1] == ' ')) return 0;
  while (*s && *s != ' ' && *s != '\n') {
    if (n >= max || *s < '0' || *s > '0' + LCELLS - 1) return -1;
    out[n++] = (uint8_t)(*s - '0');
    s++;
  }
  return n;
}

int lab_load(LabBoard *b, const char *a, const char *z, const char *x, const char *m) {
  uint8_t ta[LCELLS], tz[LCELLS], tx[LCELLS], tm[LCELLS];
  int na = parse_field(a, ta, LCELLS), nz = parse_field(z, tz, LCELLS), nx = parse_field(x, tx, LCELLS), nm = parse_field(m, tm, LCELLS);
  lab_reset(b);
  if (na < 0 || nz < 0 || nx < 0 || nm < 0) return 0;
  return lab_restore(b, ta, na, tz, nz, tx, nx, tm, nm);
}
