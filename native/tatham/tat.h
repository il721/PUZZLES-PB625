/* Shared declarations of the puzzle collection app (see README.md). */
#ifndef TAT_H
#define TAT_H

#include "inkview.h"
#include "puzzles.h"

/* the SDK 4.8.1 header (PB625) has KEY_*, newer SDKs (PB634) IV_KEY_* */
#ifndef IV_KEY_BACK
#define IV_KEY_OK KEY_OK
#define IV_KEY_MENU KEY_MENU
#define IV_KEY_PREV KEY_PREV
#define IV_KEY_NEXT KEY_NEXT
#define IV_KEY_HOME KEY_HOME
#define IV_KEY_BACK KEY_BACK
#endif

#ifndef TAT_DIR
#define TAT_DIR "/mnt/ext1/tatham"
#endif

typedef struct { int x, y, w, h; } Rect;

/* ---- fe.c: the engine's drawing API on InkView ---- */
void fe_init(void);                                   /* font names, defaults */
void fe_free(void);                                   /* closes cached fonts */
midend *fe_open(const game *g);                       /* new midend wired to the screen */
void fe_close(midend *me);
/* Fits the puzzle into the area, centred; the engine draws with this origin from now on. */
void fe_layout(midend *me, int ax, int ay, int aw, int ah, Rect *board);
void fe_load_palette(midend *me);                     /* engine colours -> grays */
void fe_set_thick(int on);
void fe_hold_updates(int on);                         /* 1: draw without refreshing the screen */
void fe_set_status_cb(void (*cb)(const char *text));  /* called from inside midend_redraw */
int fe_timer_active(void);
void fe_drain_timers(midend *me);                     /* finishes animations at once (no frames on e-ink) */
const char *fe_slug(const game *g);                   /* file-name safe id (htmlhelp_topic) */

/* ---- tr.h is separate; main.c has the UI ---- */

#endif
