/* Languages (English, Russian, German - the same set as il721/puzzles) and the user's language setting.
 *
 * A translatable text is a Str: S("English", "Русский", "Deutsch"), looked up with tr(). All texts are UTF-8.
 * The setting is "system" (follow the reader's own language) or one fixed language; it is kept in
 * /mnt/ext1/puzzlebook.cfg and is changed on the shell's Settings screen. Modules call tr() every time they
 * draw, so a change applies on the next redraw without any other plumbing. */
#ifndef I18N_H
#define I18N_H

enum { LANG_EN, LANG_RU, LANG_DE, NUM_LANGS };
enum { LANG_SYSTEM = -1 }; /* the setting only; lang() never returns it */

typedef struct { const char *t[NUM_LANGS]; } Str;
#define S(en, ru, de) {{(en), (ru), (de)}}

/* The language texts are shown in now: the setting, or the reader's language when the setting is "system". */
int lang(void);
/* The setting itself: LANG_SYSTEM, LANG_EN, LANG_RU or LANG_DE. */
int lang_setting(void);
void lang_set_setting(int setting); /* also saves it */
void lang_load(void);               /* once at start: reads the setting and the reader's language */

const char *tr(const Str *s);

#endif
