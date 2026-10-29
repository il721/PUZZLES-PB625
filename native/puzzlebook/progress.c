#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "progress.h"
#include "shell.h"

#ifndef PROGF
#define PROGF "/mnt/ext1/puzzlebook-progress.json"
#endif

#define BUFMAX (384 * 1024)
#define POOL 40000

/* ---------- a small JSON reader: the whole file as a tree of nodes in a static pool ---------- */

enum { J_NULL, J_BOOL, J_NUM, J_STR, J_OBJ, J_ARR };
typedef struct {
  unsigned char type;
  int child, next, klen, slen;
  const char *key, *str;
  long long num; /* number (integer part) or 0/1 for booleans */
} JNode;

static JNode pool[POOL];
static int npool, root = -1;
static char buf[BUFMAX + 1];
static const char *jp;
static int jbad;

static void skip_ws(void) {
  while (*jp == ' ' || *jp == '\t' || *jp == '\n' || *jp == '\r') jp++;
}

static int new_node(int type) {
  if (npool >= POOL) { jbad = 1; return -1; }
  memset(&pool[npool], 0, sizeof pool[npool]);
  pool[npool].type = (unsigned char)type;
  pool[npool].child = pool[npool].next = -1;
  return npool++;
}

/* raw string contents (escapes are skipped, not decoded: keys and ids here never need them) */
static int parse_str(const char **s, int *len) {
  if (*jp != '"') return 0;
  jp++;
  *s = jp;
  while (*jp && *jp != '"') jp += (*jp == '\\' && jp[1]) ? 2 : 1;
  if (*jp != '"') return 0;
  *len = (int)(jp - *s);
  jp++;
  return 1;
}

static int parse_value(int depth) {
  int n;
  skip_ws();
  if (depth > 20) { jbad = 1; return -1; }
  if (*jp == '{' || *jp == '[') {
    int obj = (*jp == '{'), last = -1, c;
    char close = obj ? '}' : ']';
    n = new_node(obj ? J_OBJ : J_ARR);
    if (n < 0) return -1;
    jp++;
    skip_ws();
    if (*jp == close) { jp++; return n; }
    for (;;) {
      const char *k = NULL;
      int kl = 0;
      skip_ws();
      if (obj) {
        if (!parse_str(&k, &kl)) { jbad = 1; return -1; }
        skip_ws();
        if (*jp != ':') { jbad = 1; return -1; }
        jp++;
      }
      c = parse_value(depth + 1);
      if (c < 0) { jbad = 1; return -1; }
      pool[c].key = k; pool[c].klen = kl;
      if (last < 0) pool[n].child = c; else pool[last].next = c;
      last = c;
      skip_ws();
      if (*jp == ',') { jp++; continue; }
      if (*jp == close) { jp++; return n; }
      jbad = 1;
      return -1;
    }
  }
  if (*jp == '"') {
    n = new_node(J_STR);
    if (n < 0) return -1;
    if (!parse_str(&pool[n].str, &pool[n].slen)) { jbad = 1; return -1; }
    return n;
  }
  if (!strncmp(jp, "true", 4)) { jp += 4; n = new_node(J_BOOL); if (n >= 0) pool[n].num = 1; return n; }
  if (!strncmp(jp, "false", 5)) { jp += 5; return new_node(J_BOOL); }
  if (!strncmp(jp, "null", 4)) { jp += 4; return new_node(J_NULL); }
  if (*jp == '-' || (*jp >= '0' && *jp <= '9')) {
    n = new_node(J_NUM);
    if (n < 0) return -1;
    pool[n].num = strtoll(jp, NULL, 10);
    jp++;
    while ((*jp >= '0' && *jp <= '9') || *jp == '.' || *jp == 'e' || *jp == 'E' || *jp == '+' || *jp == '-') jp++;
    return n;
  }
  jbad = 1;
  return -1;
}

static int jfind(int obj, const char *name) {
  size_t l = strlen(name);
  int c;
  if (obj < 0 || pool[obj].type != J_OBJ) return -1;
  for (c = pool[obj].child; c >= 0; c = pool[c].next)
    if ((size_t)pool[c].klen == l && !memcmp(pool[c].key, name, l)) return c;
  return -1;
}

/* ---------- export ---------- */

static void put_entry(FILE *f, const char *id, const ProgEntry *e, int last) {
  int k;
  fprintf(f, "          \"%s\": {\n", id);
  if (e->has_lists) {
    static const char *const names[PROG_LISTS] = {"chainA", "chainZ", "crosses", "marks"};
    int l;
    for (l = 0; l < PROG_LISTS; l++) {
      fprintf(f, "%s            \"%s\": [", l ? ",\n" : "", names[l]);
      for (k = 0; k < e->nlist[l]; k++) fprintf(f, "%s[%d, %d]", k ? ", " : "", e->list[l][k][0], e->list[l][k][1]);
      fprintf(f, "]");
    }
  } else if (e->ndom > 0) {
    fprintf(f, "            \"placed\": [");
    for (k = 0; k < e->ndom; k++)
      fprintf(f, "%s[[%d, %d], [%d, %d]]", k ? ", " : "", e->dom[k][0], e->dom[k][1], e->dom[k][2], e->dom[k][3]);
    fprintf(f, "]");
  } else {
    fprintf(f, "            \"cells\": {");
    for (k = 0; k < e->ncells; k++) fprintf(f, "%s\"%s\": %d", k ? ", " : "", e->cells[k].key, e->cells[k].digit);
    fprintf(f, "}");
  }
  fprintf(f, ",\n            \"elapsedMs\": 0,\n            \"checks\": 0,\n            \"solved\": %s", e->solved ? "true" : "false");
  if (e->updated_at > 0) fprintf(f, ",\n            \"updatedAt\": %lld", e->updated_at);
  fprintf(f, "\n          }%s\n", last ? "" : ",");
}

int progress_export(void) {
  FILE *f = fopen(PROGF, "w");
  time_t now = time(NULL);
  char stamp[32];
  int m, first_ns = 1;
  static ProgEntry e;
  if (!f) return PR_WRITE;
  strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", localtime(&now));
  fprintf(f, "{\n  \"bundleVersion\": 1,\n  \"exportedAt\": \"%s\",\n  \"namespaces\": {\n", stamp);
  for (m = 0; m < NUM_MODULES; m++) {
    const Module *mod = MODULES[m];
    int i, n = 0, count, touched[64], nt = 0;
    if (!mod->ns || !mod->prog_count) continue;
    count = mod->prog_count();
    for (i = 0; i < count && nt < 64; i++) { memset(&e, 0, sizeof e); if (mod->prog_get(i, &e)) touched[nt++] = i; }
    fprintf(f, "%s    \"%s\": {\n      \"puzzles\": {\n", first_ns ? "" : ",\n", mod->ns);
    first_ns = 0;
    for (n = 0; n < nt; n++) {
      memset(&e, 0, sizeof e);
      mod->prog_get(touched[n], &e);
      put_entry(f, mod->prog_id(touched[n]), &e, n == nt - 1);
    }
    fprintf(f, "      }\n    }");
  }
  fprintf(f, "\n  }\n}\n");
  if (ferror(f)) { fclose(f); return PR_WRITE; }
  return fclose(f) == 0 ? PR_OK : PR_WRITE;
}

/* ---------- import ---------- */

void progress_discard(void) { npool = 0; root = -1; }

int progress_open(char *exported_at, int n) {
  FILE *f = fopen(PROGF, "r");
  size_t len;
  int v, d;
  progress_discard();
  if (n > 0) exported_at[0] = 0;
  if (!f) return PR_NOFILE;
  len = fread(buf, 1, BUFMAX + 1, f);
  fclose(f);
  if (len > BUFMAX) return PR_BAD;
  buf[len] = 0;
  jp = buf; jbad = 0;
  root = parse_value(0);
  skip_ws();
  if (root < 0 || jbad || *jp || pool[root].type != J_OBJ) { progress_discard(); return PR_BAD; }
  v = jfind(root, "bundleVersion");
  if (v < 0 || pool[v].type != J_NUM) { progress_discard(); return PR_BAD; }
  if (pool[v].num > 1) { progress_discard(); return PR_NEWER; }
  d = jfind(root, "exportedAt");
  if (d >= 0 && pool[d].type == J_STR && n > 0) {
    int i, l = pool[d].slen < 16 ? pool[d].slen : 16;
    if (l > n - 1) l = n - 1;
    memcpy(exported_at, pool[d].str, (size_t)l);
    exported_at[l] = 0;
    for (i = 0; i < l; i++) if (exported_at[i] == 'T') exported_at[i] = ' ';
  }
  return PR_OK;
}

/* the source's mergePuzzleEntry, as far as this app tracks it: solved beats unsolved; two solved copies tie;
 * between two unsolved copies the later updatedAt wins (a stamp beats none); a tie keeps the local copy */
static int incoming_wins(const ProgEntry *loc, const ProgEntry *in) {
  if (loc->solved != in->solved) return in->solved;
  if (loc->solved) return 0;
  if (loc->updated_at == in->updated_at) return 0;
  if (loc->updated_at == 0) return 1;
  if (in->updated_at == 0) return 0;
  return in->updated_at > loc->updated_at;
}

static int read_entry(int node, ProgEntry *e) {
  int s = jfind(node, "solved"), u = jfind(node, "updatedAt"), c = jfind(node, "cells"), pl = jfind(node, "placed"), k;
  memset(e, 0, sizeof *e);
  if (pool[node].type != J_OBJ) return 0;
  e->solved = (s >= 0 && pool[s].type == J_BOOL && pool[s].num) ? 1 : 0;
  if (u >= 0 && pool[u].type == J_NUM && pool[u].num > 0) e->updated_at = pool[u].num;
  if (c >= 0 && pool[c].type == J_OBJ)
    for (k = pool[c].child; k >= 0 && e->ncells < PROG_MAX_CELLS; k = pool[k].next) {
      ProgCell *pc = &e->cells[e->ncells];
      if (pool[k].type != J_NUM || pool[k].num < 0 || pool[k].num > 9 || pool[k].klen < 1 || pool[k].klen >= (int)sizeof pc->key) continue;
      memcpy(pc->key, pool[k].key, (size_t)pool[k].klen);
      pc->key[pool[k].klen] = 0;
      pc->digit = (int)pool[k].num;
      e->ncells++;
    }
  /* "placed": [[[r,c],[r,c]], ...]; entries that are not two number pairs are skipped (the module validates the cells) */
  if (pl >= 0 && pool[pl].type == J_ARR)
    for (k = pool[pl].child; k >= 0 && e->ndom < PROG_MAX_DOMS; k = pool[k].next) {
      int a, b, v[4], n = 0, ok = pool[k].type == J_ARR;
      for (a = ok ? pool[k].child : -1; a >= 0 && ok; a = pool[a].next) {
        if (pool[a].type != J_ARR) { ok = 0; break; }
        for (b = pool[a].child; b >= 0; b = pool[b].next) {
          if (pool[b].type != J_NUM || n >= 4 || pool[b].num < 0 || pool[b].num > 255) { ok = 0; break; }
          v[n++] = (int)pool[b].num;
        }
      }
      if (!ok || n != 4) continue;
      for (a = 0; a < 4; a++) e->dom[e->ndom][a] = (unsigned char)v[a];
      e->ndom++;
    }
  /* "chainA", "chainZ", "crosses", "marks": [[r,c], ...]; a list that is missing or malformed is empty, and the module
   * rejects the whole entry if the lists do not make a valid board (like the source's fromJson) */
  {
    static const char *const names[PROG_LISTS] = {"chainA", "chainZ", "crosses", "marks"};
    int l;
    for (l = 0; l < PROG_LISTS; l++) {
      int ln = jfind(node, names[l]), a, b, ok = ln >= 0 && pool[ln].type == J_ARR;
      if (!ok) continue;
      e->has_lists = 1;
      for (a = pool[ln].child; a >= 0 && e->nlist[l] < PROG_MAX_LIST; a = pool[a].next) {
        int v[2], n = 0, good = pool[a].type == J_ARR;
        for (b = good ? pool[a].child : -1; b >= 0; b = pool[b].next) {
          if (pool[b].type != J_NUM || n >= 2 || pool[b].num < 0 || pool[b].num > 255) { good = 0; break; }
          v[n++] = (int)pool[b].num;
        }
        if (!good || n != 2) continue;
        e->list[l][e->nlist[l]][0] = (unsigned char)v[0];
        e->list[l][e->nlist[l]][1] = (unsigned char)v[1];
        e->nlist[l]++;
      }
    }
  }
  return e->solved || e->ncells > 0 || e->ndom > 0 || e->nlist[0] > 1 || e->nlist[1] > 1 || e->nlist[2] > 0 || e->nlist[3] > 0; /* an entry with nothing in it is not progress */
}

void progress_apply(int *added, int *updated) {
  static ProgEntry loc, in;
  int m, nsroot;
  *added = *updated = 0;
  if (root < 0) return;
  nsroot = jfind(root, "namespaces");
  for (m = 0; m < NUM_MODULES; m++) {
    const Module *mod = MODULES[m];
    int i, count, puzzles;
    if (!mod->ns || !mod->prog_count) continue;
    puzzles = jfind(jfind(nsroot, mod->ns), "puzzles");
    if (puzzles < 0) continue;
    count = mod->prog_count();
    for (i = 0; i < count; i++) {
      int en = jfind(puzzles, mod->prog_id(i));
      if (en < 0 || !read_entry(en, &in)) continue;
      memset(&loc, 0, sizeof loc);
      if (!mod->prog_get(i, &loc)) { mod->prog_put(i, &in); (*added)++; }
      else if (incoming_wins(&loc, &in)) { mod->prog_put(i, &in); (*updated)++; }
    }
  }
}
