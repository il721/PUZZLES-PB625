#include <stdio.h>
#include <string.h>

#include "i18n.h"

#ifndef CFGF
#define CFGF "/mnt/ext1/puzzlebook.cfg"
#endif
/* The firmware keeps its own language here as a line `language=ru` (checked by the log line written at start). */
#ifndef SYSCFGF
#define SYSCFGF "/mnt/ext1/system/config/global.cfg"
#endif

static int setting = LANG_SYSTEM, system_lang = LANG_EN;

static int code_to_lang(const char *c) {
  if (strncmp(c, "ru", 2) == 0) return LANG_RU;
  if (strncmp(c, "de", 2) == 0) return LANG_DE;
  return LANG_EN; /* like the source app: anything unsupported falls back to English */
}

/* value of `key=` in a plain text file, or NULL */
static const char *cfg_value(FILE *f, const char *key, char *line, int n) {
  size_t kl = strlen(key);
  while (fgets(line, n, f))
    if (strncmp(line, key, kl) == 0 && line[kl] == '=') return line + kl + 1;
  return NULL;
}

void lang_load(void) {
  char line[128];
  const char *v;
  FILE *f = fopen(SYSCFGF, "r");
  if (f) {
    v = cfg_value(f, "language", line, sizeof line);
    if (v) system_lang = code_to_lang(v);
    fclose(f);
  }
  f = fopen(CFGF, "r");
  if (f) {
    v = cfg_value(f, "lang", line, sizeof line);
    if (v && strncmp(v, "system", 6) != 0) setting = code_to_lang(v);
    fclose(f);
  }
}

int lang_setting(void) { return setting; }

int lang(void) { return setting == LANG_SYSTEM ? system_lang : setting; }

void lang_set_setting(int s) {
  static const char *const CODE[NUM_LANGS] = {"en", "ru", "de"};
  FILE *f;
  setting = (s >= 0 && s < NUM_LANGS) ? s : LANG_SYSTEM;
  f = fopen(CFGF, "w");
  if (!f) return;
  fprintf(f, "lang=%s\n", setting == LANG_SYSTEM ? "system" : CODE[setting]);
  fclose(f);
}

const char *tr(const Str *s) { return s->t[lang()]; }
