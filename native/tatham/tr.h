/* Languages for the puzzle collection: English, Russian, German.
 * The puzzles themselves (Simon Tatham's code) are English only; their preset names, option labels and
 * status-bar texts go through tr_glossary(), which replaces known words and phrases and leaves the rest as is. */
#ifndef TAT_TR_H
#define TAT_TR_H

#include <stddef.h>

enum { LANG_EN, LANG_RU, LANG_DE, NUM_LANGS };
#define LANG_SYSTEM (-1)

/* UI strings of the app itself. Keep in step with the table in tr.c. */
enum {
    S_TITLE, S_CREDIT, S_GAMES, S_NEW, S_UNDO, S_REDO, S_MENU, S_BTN_LEFT, S_BTN_RIGHT,
    S_RESTART, S_SOLVE, S_TYPE, S_CUSTOM, S_RULES, S_LANGUAGE, S_CLEAN, S_THICK_ON, S_THICK_OFF,
    S_EXIT, S_BACK, S_OK, S_CANCEL, S_PAGE, S_GENERATING, S_SOLVED, S_SYSTEM, S_NEW_Q, S_YES, S_NO,
    S_TIP, S_PREV, S_NEXT, S_SAVED, S_ERROR, S_CUSTOM_TITLE, S_HOME_SAVED, S_HOME_DONE, S_SOLVE_NO,
    S_COUNT
};

void tr_load(void);                     /* reads the system language and the app's setting */
int tr_lang(void);                      /* effective language: LANG_EN / LANG_RU / LANG_DE */
int tr_setting(void);                   /* LANG_SYSTEM or a language */
void tr_set_setting(int s);             /* remembers it in the app's config file */
const char *T(int id);                  /* UI string in the current language */

/* puzzle name in the current language (en = the engine's name, e.g. "Light Up"), or en itself */
const char *tr_game_name(const char *en);
/* rules text for a puzzle (slug = htmlhelp_topic, e.g. "lightup") in the current language */
const char *tr_rules(const char *slug);
/* en with known words replaced, always written to out (which is returned); English copies en as it is */
const char *tr_glossary(const char *en, char *out, size_t n);

/* tiny key=value config shared by the app (lang, last game, thick lines) */
const char *cfg_path(void);
int cfg_get(const char *key, char *out, size_t n);   /* 1 if found */
void cfg_set(const char *key, const char *value);

#endif
