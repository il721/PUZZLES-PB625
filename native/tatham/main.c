/* Puzzle Collection for PocketBook e-readers (PB625, PB634): Simon Tatham's puzzles with a touch UI.
 * Screens: home (list of puzzles), play (board, keypad, undo/redo), menu, size/difficulty, custom settings,
 * rules, language. See README.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "tat.h"
#include "tr.h"
#include "savezip.h"

enum { SC_HOME, SC_PLAY, SC_LIST, SC_RULES };
enum { LK_MENU, LK_HOME_MENU, LK_PRESETS, LK_LANG, LK_CONFIG, LK_CONFIRM };
enum {
    B_NONE = 0, B_GAMES, B_NEW, B_UNDO, B_REDO, B_MODE, B_MENU, B_PREV, B_NEXT, B_BACK, B_OK, B_CANCEL, B_YES, B_NO,
    B_ITEM = 100, B_GAME = 200, B_KEY = 300
};
enum {
    M_NEW = 1, M_RESTART, M_SOLVE, M_TYPE, M_RULES, M_LANG, M_THICK, M_CLEAN, M_HOME, M_EXIT, M_EXPORT, M_IMPORT,
    M_CUSTOM = 50, LG_SYSTEM = 60, LG_EN, LG_RU, LG_DE
};
enum { PT_NONE, PT_PENDING, PT_DRAG, PT_RIGHT, PT_IGNORE };   /* PT_IGNORE: the engine did not use the press */

typedef struct { int x, y, w, h, id; } Btn;
typedef struct { char text[400]; int id, selected, disabled; } Item;

#define MAXBTN 128
#define MAXITEM 96
static Btn btns[MAXBTN];
static int nbtn;
static Item items[MAXITEM];
static int nitems, list_page, list_kind;
static char list_title[160];

static int W, H;
static ifont *f_title, *f_btn, *f_text, *f_small;
static int screen = SC_HOME, back_screen = SC_HOME, home_page;
static int updates;

/* the open puzzle */
static midend *me;
static const game *cur;
static int cur_idx = -1;
static char cur_slug[48];
static key_label *keys;
static int nkeys;
static int rmode, solved, can_undo, can_redo, thick;
static int board_updates, painting;
static char status_text[256];
static Rect area, board;
static int top_h, status_h, bottom_h, keypad_h;

/* preset browsing and custom settings */
static struct preset_menu *pm_stack[4];
static int pm_depth;
static config_item *cfg_items;
static char *cfg_title;
static int cfg_kb_item;
static char kbbuf[64];
static char message[200];

/* pointer on the board */
static struct { int state, on_board, btn_id, dx, dy; } ptr;

static int sc(int v)
{
    int r = v * W / 1072;
    return r < 1 ? 1 : r;
}

/* ---- files ---- */

static void path_of(char *out, size_t n, const char *slug, const char *ext)
{
    snprintf(out, n, "%s/%s.%s", TAT_DIR, slug, ext);
}

static void wr(void *ctx, const void *buf, int len) { fwrite(buf, 1, len, (FILE *)ctx); }
static bool rd(void *ctx, void *buf, int len) { return fread(buf, 1, len, (FILE *)ctx) == (size_t)len; }

static void save_game(void)
{
    char p[256], tmp[256], st[256];
    FILE *f;
    if (!me) return;
    path_of(p, sizeof p, cur_slug, "sav");
    path_of(tmp, sizeof tmp, cur_slug, "tmp");
    f = fopen(tmp, "wb");
    if (!f) return;
    midend_serialise(me, wr, f);
    if (fclose(f) != 0) return;
    rename(tmp, p);
    path_of(st, sizeof st, cur_slug, "st");
    f = fopen(st, "w");
    if (f) { fputc(midend_status(me) > 0 ? '1' : '0', f); fclose(f); }
}

static int load_game(void)
{
    char p[256];
    const char *err;
    FILE *f;
    path_of(p, sizeof p, cur_slug, "sav");
    f = fopen(p, "rb");
    if (!f) return 0;
    err = midend_deserialise(me, rd, f);
    fclose(f);
    return err == NULL;
}

/* 0 = no save, 1 = in progress, 2 = solved */
static int save_state(const char *slug)
{
    char p[256];
    FILE *f;
    int c;
    path_of(p, sizeof p, slug, "sav");
    f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    path_of(p, sizeof p, slug, "st");
    f = fopen(p, "r");
    if (!f) return 1;
    c = fgetc(f);
    fclose(f);
    return c == '1' ? 2 : 1;
}

/* ---- fonts and drawing helpers ---- */

/* rules page: fonts of any size, opened and closed on the spot */
static ifont *open_font(const char *bold, const char *plain, int size)
{
    ifont *f = OpenFont(bold ? bold : plain, size, 1);
    if (!f) f = OpenFont("LiberationSans", size, 1);
    return f;
}

/* everything else: a cache of the sizes in use (the fit helper tries several per text) */
#define NUIFONTS 64
static struct { int size, bold; ifont *f; } uif[NUIFONTS];
static int nuif;

static ifont *ui_font(int bold, int size)
{
    int i;
    ifont *f;
    for (i = 0; i < nuif; i++)
        if (uif[i].size == size && uif[i].bold == bold) return uif[i].f;
    if (nuif == NUIFONTS) return uif[0].f;
    f = open_font(bold ? "LiberationSans-Bold" : NULL, "LiberationSans", size);
    uif[nuif].size = size; uif[nuif].bold = bold; uif[nuif].f = f;
    nuif++;
    return f;
}

static void init_fonts(void)
{
    int i;
    for (i = 0; i < nuif; i++)
        if (uif[i].f) CloseFont(uif[i].f);
    nuif = 0;
    f_title = ui_font(1, sc(50));
    f_btn = ui_font(1, sc(40));
    f_text = ui_font(0, sc(40));
    f_small = ui_font(0, sc(32));
}

static void text_in(ifont *f, int color, int x, int y, int w, int h, const char *s, int flags)
{
    if (!f) return;                                   /* no font at all: nothing sensible to draw */
    SetFont(f, color);
    DrawTextRect(x, y, w, h, s, flags);
}

/* One line in the largest size from maxs down to mins that fits the width; when even the smallest does
 * not fit, the text wraps inside the rectangle or is cut with "..." (flags). */
static void text_fit(int bold, int maxs, int mins, int color, int x, int y, int w, int h, const char *s, int flags)
{
    int size;
    ifont *f = NULL;
    for (size = maxs; size >= mins; size -= 2) {
        f = ui_font(bold, size);
        if (!f) return;
        SetFont(f, color);
        if (StringWidth(s) <= w) break;
    }
    if (!f) return;
    DrawTextRect(x, y, w, h, s, flags);
}

static void add_btn(int x, int y, int w, int h, int id)
{
    if (nbtn < MAXBTN) { btns[nbtn].x = x; btns[nbtn].y = y; btns[nbtn].w = w; btns[nbtn].h = h; btns[nbtn].id = id; nbtn++; }
}

static void box(int x, int y, int w, int h, int thick_px)
{
    FillArea(x, y, w, thick_px, BLACK);
    FillArea(x, y + h - thick_px, w, thick_px, BLACK);
    FillArea(x, y, thick_px, h, BLACK);
    FillArea(x + w - thick_px, y, thick_px, h, BLACK);
}

/* a framed button that is also registered as tap target */
static void button(int x, int y, int w, int h, const char *text, int id, int enabled, int dark)
{
    FillArea(x, y, w, h, dark ? BLACK : WHITE);
    box(x, y, w, h, 2);
    text_fit(1, sc(40), sc(26), dark ? WHITE : (enabled ? BLACK : LGRAY), x + 4, y, w - 8, h, text,
             ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    if (enabled) add_btn(x, y, w, h, id);
}

static void refresh(int full)
{
    if (full || ++updates % 5 == 0) FullUpdate();
    else PartialUpdate(0, 0, W, H);
}

static void clear_screen(void)
{
    SetClip(0, 0, W, H);
    FillArea(0, 0, W, H, WHITE);
    nbtn = 0;
}

/* ---- layout of the play screen ---- */

static void layout(void)
{
    int rows = 0, per_row;
    top_h = sc(110);
    status_h = sc(66);
    bottom_h = sc(124);
    keypad_h = 0;
    if (nkeys > 0) {
        per_row = nkeys > 10 ? (nkeys + 1) / 2 : nkeys;
        rows = (nkeys + per_row - 1) / per_row;
        keypad_h = rows * sc(92) + sc(8);
    }
    area.x = sc(8);
    area.y = top_h + status_h;
    area.w = W - sc(16);
    area.h = H - top_h - status_h - bottom_h - keypad_h - sc(8);
    if (area.h < 100) area.h = 100;
}

static void draw_status(void)
{
    int y = top_h;
    char buf[320];
    const char *t = status_text;
    SetClip(0, 0, W, H);
    FillArea(0, y, W, status_h, WHITE);
    if (t[0] && !message[0]) {
        tr_glossary(t, buf, sizeof buf);
        text_fit(0, sc(38), sc(24), BLACK, sc(16), y, solved ? W * 6 / 10 : W - sc(32), status_h, buf,
                 ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    }
    if (message[0])
        text_fit(0, sc(38), sc(24), BLACK, sc(16), y, W - sc(32), status_h, message, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (solved)
        text_fit(1, sc(40), sc(28), BLACK, W * 6 / 10, y, W * 4 / 10 - sc(16), status_h, T(S_SOLVED), ALIGN_RIGHT | VALIGN_MIDDLE);
}

static void draw_bottom(void)
{
    int y = H - bottom_h, bw = (W - sc(16) * 5) / 4, x = sc(16);
    SetClip(0, 0, W, H);
    FillArea(0, y - 2, W, bottom_h + 2, WHITE);
    button(x, y, bw, bottom_h - sc(12), T(S_UNDO), B_UNDO, can_undo, 0);
    x += bw + sc(16);
    button(x, y, bw, bottom_h - sc(12), T(S_REDO), B_REDO, can_redo, 0);
    x += bw + sc(16);
    button(x, y, bw, bottom_h - sc(12), rmode ? T(S_BTN_RIGHT) : T(S_BTN_LEFT), B_MODE, 1, rmode);
    x += bw + sc(16);
    button(x, y, bw, bottom_h - sc(12), T(S_MENU), B_MENU, 1, 0);
}

/* keys with a word on them ("Clear", "Сброс") get half as much width again as a digit */
static int key_units(const char *lab)
{
    int n = 0;
    for (; *lab; lab++) if (((unsigned char)*lab & 0xC0) != 0x80) n++;   /* UTF-8 characters */
    return n > 2 ? 3 : 2;
}

static void draw_keypad(void)
{
    int i, per_row, rows, r, y0;
    if (nkeys <= 0) return;
    per_row = nkeys > 10 ? (nkeys + 1) / 2 : nkeys;
    rows = (nkeys + per_row - 1) / per_row;
    y0 = H - bottom_h - keypad_h;
    for (r = 0; r < rows; r++) {
        int first = r * per_row, last = first + per_row < nkeys ? first + per_row : nkeys;
        int units = 0, avail, x = sc(8), used = 0;
        char lab[64];
        for (i = first; i < last; i++) {
            tr_glossary(keys[i].label ? keys[i].label : "?", lab, sizeof lab);
            units += key_units(lab);
        }
        avail = W - sc(16) - (last - first - 1) * sc(8);
        for (i = first; i < last; i++) {
            int u, kw;
            tr_glossary(keys[i].label ? keys[i].label : "?", lab, sizeof lab);
            u = key_units(lab);
            used += u;
            kw = avail * used / units - avail * (used - u) / units;   /* no rounding gaps */
            button(x, y0 + r * sc(92), kw, sc(84), lab, B_KEY + i, 1, 0);
            x += kw + sc(8);
        }
    }
}

static void draw_top(void)
{
    char title[200];
    SetClip(0, 0, W, H);
    FillArea(0, 0, W, top_h, WHITE);
    button(sc(12), sc(10), sc(190), top_h - sc(20), T(S_GAMES), B_GAMES, 1, 0);
    button(W - sc(12) - sc(160), sc(10), sc(160), top_h - sc(20), T(S_NEW), B_NEW, 1, 0);
    snprintf(title, sizeof title, "%s", tr_game_name(cur->name));
    text_fit(1, sc(50), sc(30), BLACK, sc(212), 0, W - sc(212) - sc(182), top_h, title, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    FillArea(0, top_h - 3, W, 2, BLACK);
}

static void paint_board(void)
{
    SetClip(0, 0, W, H);
    FillArea(0, area.y, W, area.h, WHITE);
    fe_hold_updates(1);
    painting = 1;
    midend_force_redraw(me);
    painting = 0;
    fe_hold_updates(0);
}

static void paint_play(void)
{
    nbtn = 0;
    clear_screen();
    draw_top();
    draw_status();
    draw_keypad();
    draw_bottom();
    paint_board();
}

static void on_status(const char *text)
{
    snprintf(status_text, sizeof status_text, "%s", text);
    if (screen != SC_PLAY) return;
    draw_status();
    if (!painting) PartialUpdate(0, top_h, W, status_h);
}

/* ---- puzzle life cycle ---- */

static void sync_flags(int *changed)
{
    int u = midend_can_undo(me) ? 1 : 0, r = midend_can_redo(me) ? 1 : 0, s = midend_status(me) > 0 ? 1 : 0;
    *changed = (u != can_undo) || (r != can_redo) || (s != solved);
    can_undo = u; can_redo = r; solved = s;
}

static void setup_play(void)
{
    int ch;
    fe_load_palette(me);
    if (keys) free_keys(keys, nkeys);
    keys = midend_request_keys(me, &nkeys);
    layout();
    fe_layout(me, area.x, area.y, area.w, area.h, &board);
    sync_flags(&ch);
    message[0] = 0;       /* status_text stays: the engine only repeats it when it changes */
    board_updates = 0;
}

static void overlay(const char *text)
{
    int bw = W * 7 / 10, bh = sc(150), x = (W - bw) / 2, y = (H - bh) / 2;
    SetClip(0, 0, W, H);
    FillArea(x, y, bw, bh, WHITE);
    box(x, y, bw, bh, 4);
    text_in(f_btn, BLACK, x + 10, y, bw - 20, bh, text, ALIGN_CENTER | VALIGN_MIDDLE);
    PartialUpdate(x, y, bw, bh);
}

static void close_game(void)
{
    if (!me) return;
    save_game();
    fe_close(me);
    me = NULL;
    if (keys) { free_keys(keys, nkeys); keys = NULL; nkeys = 0; }
}

static void new_game(void)
{
    overlay(T(S_GENERATING));
    midend_new_game(me);
    setup_play();
    save_game();
    screen = SC_PLAY;
    paint_play();
    refresh(1);
}

static void open_game(int idx)
{
    close_game();
    cur_idx = idx;
    cur = gamelist[idx];
    snprintf(cur_slug, sizeof cur_slug, "%s", fe_slug(cur));
    cfg_set("last", cur_slug);
    status_text[0] = 0;
    me = fe_open(cur);
    fe_set_status_cb(on_status);
    rmode = 0;
    if (load_game()) {
        setup_play();
        screen = SC_PLAY;
        paint_play();
        refresh(1);
    } else {
        new_game();
    }
}

/* ---- screens ---- */

static void go_home(void)
{
    close_game();
    screen = SC_HOME;
}

static void paint_home(void)
{
    int rows, per_page, i, cw, ch = sc(112), top = sc(110), foot = sc(120), n = gamecount, pages, first;
    char buf[64];
    clear_screen();
    text_fit(1, sc(50), sc(30), BLACK, sc(16), 0, W - sc(200), top, T(S_TITLE), ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    button(W - sc(12) - sc(170), sc(14), sc(170), top - sc(28), T(S_MENU), B_MENU, 1, 0);
    FillArea(0, top - 3, W, 2, BLACK);
    rows = (H - top - foot) / ch;
    if (rows < 1) rows = 1;
    per_page = rows * 2;
    pages = (n + per_page - 1) / per_page;
    if (home_page >= pages) home_page = pages - 1;
    if (home_page < 0) home_page = 0;
    first = home_page * per_page;
    cw = (W - sc(16) * 3) / 2;
    for (i = 0; i < per_page && first + i < n; i++) {
        int idx = first + i, x = sc(16) + (i % 2) * (cw + sc(16)), y = top + sc(10) + (i / 2) * ch, st;
        const char *slug = fe_slug(gamelist[idx]);
        FillArea(x, y, cw, ch - sc(12), WHITE);
        box(x, y, cw, ch - sc(12), 2);
        st = save_state(slug);
        text_fit(1, sc(40), sc(26), BLACK, x + 8, y + (st ? sc(6) : 0), cw - 16, st ? (ch - sc(12)) * 6 / 10 : ch - sc(12),
                 tr_game_name(gamelist[idx]->name), ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
        if (st)
            text_fit(0, sc(30), sc(22), st == 2 ? BLACK : DGRAY, x + 8, y + (ch - sc(12)) * 58 / 100, cw - 16,
                     (ch - sc(12)) * 38 / 100, st == 2 ? T(S_HOME_DONE) : T(S_HOME_SAVED),
                     ALIGN_CENTER | VALIGN_MIDDLE);
        add_btn(x, y, cw, ch - sc(12), B_GAME + idx);
    }
    FillArea(0, H - foot, W, 2, BLACK);
    if (pages > 1) {
        button(sc(16), H - foot + sc(12), sc(220), foot - sc(24), T(S_PREV), B_PREV, home_page > 0, 0);
        button(W - sc(16) - sc(220), H - foot + sc(12), sc(220), foot - sc(24), T(S_NEXT), B_NEXT, home_page < pages - 1, 0);
        snprintf(buf, sizeof buf, "%s %d/%d", T(S_PAGE), home_page + 1, pages);
        text_in(f_text, BLACK, sc(250), H - foot, W - sc(500), foot * 6 / 10, buf, ALIGN_CENTER | VALIGN_MIDDLE);
        text_in(f_small, DGRAY, sc(250), H - foot + foot * 5 / 10, W - sc(500), foot * 5 / 10, "S. Tatham, MIT", ALIGN_CENTER | VALIGN_MIDDLE);
    } else {
        text_fit(0, sc(32), sc(20), DGRAY, sc(16), H - foot, W - sc(32), foot, T(S_CREDIT), ALIGN_CENTER | VALIGN_MIDDLE);
    }
}

/* generic list */

static void list_begin(int kind, const char *title)
{
    list_kind = kind;
    nitems = 0;
    snprintf(list_title, sizeof list_title, "%s", title);
}

static void list_add(int id, const char *text, int selected, int disabled)
{
    if (nitems >= MAXITEM) return;
    items[nitems].id = id;
    items[nitems].selected = selected;
    items[nitems].disabled = disabled;
    snprintf(items[nitems].text, sizeof items[nitems].text, "%s", text);
    nitems++;
}

static void paint_list(void)
{
    int top = sc(110), foot = sc(130), rowh = sc(108), rows, pages, first, i;
    clear_screen();
    text_fit(1, sc(50), sc(30), BLACK, sc(16), 0, W - sc(32), top, list_title, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    FillArea(0, top - 3, W, 2, BLACK);
    if (list_kind == LK_CONFIRM) {
        text_in(f_text, BLACK, sc(40), top + sc(120), W - sc(80), sc(300), items[0].text, ALIGN_CENTER | VALIGN_TOP);
        button(sc(60), top + sc(480), (W - sc(180)) / 2, sc(130), T(S_YES), B_YES, 1, 0);
        button(W - sc(60) - (W - sc(180)) / 2, top + sc(480), (W - sc(180)) / 2, sc(130), T(S_NO), B_NO, 1, 0);
        return;
    }
    rows = (H - top - foot) / rowh;
    if (rows < 1) rows = 1;
    pages = (nitems + rows - 1) / rows;
    if (pages < 1) pages = 1;
    if (list_page >= pages) list_page = pages - 1;
    if (list_page < 0) list_page = 0;
    first = list_page * rows;
    for (i = 0; i < rows && first + i < nitems; i++) {
        Item *it = &items[first + i];
        int y = top + sc(8) + i * rowh;
        char line[420];
        snprintf(line, sizeof line, "%s%s", it->selected ? "* " : "", it->text);
        FillArea(sc(16), y, W - sc(32), rowh - sc(10), WHITE);
        box(sc(16), y, W - sc(32), rowh - sc(10), 2);
        text_fit(0, sc(40), sc(28), it->disabled ? LGRAY : BLACK, sc(32), y, W - sc(64), rowh - sc(10), line,
                 ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
        if (!it->disabled) add_btn(sc(16), y, W - sc(32), rowh - sc(10), B_ITEM + i);
    }
    FillArea(0, H - foot, W, 2, BLACK);
    if (list_kind == LK_CONFIG) {
        button(sc(16), H - foot + sc(14), (W - sc(48)) / 2, foot - sc(28), T(S_OK), B_OK, 1, 1);
        button(sc(32) + (W - sc(48)) / 2, H - foot + sc(14), (W - sc(48)) / 2, foot - sc(28), T(S_CANCEL), B_CANCEL, 1, 0);
    } else {
        int bw = pages > 1 ? sc(210) : W - sc(32);
        button(sc(16), H - foot + sc(14), bw, foot - sc(28), T(S_BACK), B_BACK, 1, 0);
        if (pages > 1) {
            char buf[64];
            button(sc(16) + bw + sc(12), H - foot + sc(14), sc(210), foot - sc(28), T(S_PREV), B_PREV, list_page > 0, 0);
            button(W - sc(16) - sc(210), H - foot + sc(14), sc(210), foot - sc(28), T(S_NEXT), B_NEXT, list_page < pages - 1, 0);
            int tx = sc(16) + bw + sc(12) + sc(210), tw = (W - sc(16) - sc(210)) - tx;
            snprintf(buf, sizeof buf, "%d/%d", list_page + 1, pages);
            text_in(f_text, BLACK, tx, H - foot, tw, foot, buf, ALIGN_CENTER | VALIGN_MIDDLE);
        }
    }
    (void)pages;
}

static void paint_rules(void)
{
    int top = sc(110), foot = sc(130), tw = W - sc(64), avail = H - top - foot - sc(60), size, h1 = 0, h2 = 0, gap;
    const char *rules = tr_rules(fe_slug(cur)), *tip = T(S_TIP);
    ifont *f = NULL, *g = NULL;
    char name[160];
    clear_screen();
    snprintf(name, sizeof name, "%s", tr_game_name(cur->name));
    text_fit(1, sc(50), sc(30), BLACK, sc(16), 0, W - sc(32), top, name, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    FillArea(0, top - 3, W, 2, BLACK);
    /* the rules in the largest font that still fits with the tip below them */
    for (size = sc(80); ; size -= sc(4)) {
        int last = size - sc(4) < sc(30);
        f = open_font(NULL, "LiberationSans", size);
        g = open_font(NULL, "LiberationSans", size * 3 / 4);
        gap = size / 2;
        if (f && g) {
            SetFont(f, BLACK);
            h1 = TextRectHeight(tw, rules, ALIGN_LEFT);
            SetFont(g, DGRAY);
            h2 = TextRectHeight(tw, tip, ALIGN_LEFT);
            if (h1 + gap + h2 <= avail || last) break;
        } else if (last) break;
        if (f) CloseFont(f);
        if (g) CloseFont(g);
        f = g = NULL;
    }
    if (f && g) {
        text_in(f, BLACK, sc(32), top + sc(24), tw, h1 + 4, rules, ALIGN_LEFT | VALIGN_TOP);
        text_in(g, DGRAY, sc(32), top + sc(24) + h1 + gap, tw, h2 + 4, tip, ALIGN_LEFT | VALIGN_TOP);
    } else {                                          /* fonts missing: the fixed sizes */
        int h = TextRectHeight(tw, rules, ALIGN_LEFT);
        text_in(f_text, BLACK, sc(32), top + sc(24), tw, h + 4, rules, ALIGN_LEFT | VALIGN_TOP);
        text_in(f_small, DGRAY, sc(32), top + sc(24) + h + sc(30), tw, avail - h, tip, ALIGN_LEFT | VALIGN_TOP);
    }
    if (f) CloseFont(f);
    if (g) CloseFont(g);
    FillArea(0, H - foot, W, 2, BLACK);
    button(sc(16), H - foot + sc(14), W - sc(32), foot - sc(28), T(S_BACK), B_BACK, 1, 0);
}

static void paint(int full)
{
    switch (screen) {
      case SC_HOME: paint_home(); break;
      case SC_PLAY: paint_play(); break;
      case SC_LIST: paint_list(); break;
      case SC_RULES: paint_rules(); break;
    }
    refresh(full);
}

/* ---- list contents ---- */

static void show_list(void)
{
    if (screen != SC_LIST) back_screen = screen;
    screen = SC_LIST;
}

static void open_game_menu(void)
{
    char buf[200];
    list_begin(LK_MENU, tr_game_name(cur->name));
    list_add(M_NEW, T(S_NEW), 0, 0);
    list_add(M_RESTART, T(S_RESTART), 0, 0);
    if (cur->can_solve) list_add(M_SOLVE, T(S_SOLVE), 0, 0);
    list_add(M_TYPE, T(S_TYPE), 0, 0);
    list_add(M_RULES, T(S_RULES), 0, 0);
    list_add(M_THICK, thick ? T(S_THICK_ON) : T(S_THICK_OFF), 0, 0);
    list_add(M_LANG, T(S_LANGUAGE), 0, 0);
    list_add(M_CLEAN, T(S_CLEAN), 0, 0);
    list_add(M_HOME, T(S_GAMES), 0, 0);
    list_add(M_EXIT, T(S_EXIT), 0, 0);
    (void)buf;
    list_page = 0;
    show_list();
}

static void open_home_menu(void)
{
    list_begin(LK_HOME_MENU, T(S_MENU));
    list_add(M_LANG, T(S_LANGUAGE), 0, 0);
    list_add(M_THICK, thick ? T(S_THICK_ON) : T(S_THICK_OFF), 0, 0);
    list_add(M_EXPORT, T(S_EXPORT), 0, 0);
    list_add(M_IMPORT, T(S_IMPORT), 0, 0);
    list_add(M_CLEAN, T(S_CLEAN), 0, 0);
    list_add(M_EXIT, T(S_EXIT), 0, 0);
    list_page = 0;
    show_list();
}

static void fill_presets(void)
{
    struct preset_menu *m = pm_stack[pm_depth];
    int i, cur_id = midend_which_preset(me);
    char buf[260], t[200];
    list_begin(LK_PRESETS, T(S_TYPE));
    for (i = 0; i < m->n_entries; i++) {
        struct preset_menu_entry *e = &m->entries[i];
        tr_glossary(e->title, t, sizeof t);
        if (e->submenu) snprintf(buf, sizeof buf, "%s  >", t);
        else snprintf(buf, sizeof buf, "%s", t);
        list_add(i, buf, !e->submenu && e->id == cur_id, 0);
    }
    if (pm_depth == 0) list_add(M_CUSTOM, T(S_CUSTOM), 0, 0);
}

static void open_presets(void)
{
    int idl;
    pm_depth = 0;
    pm_stack[0] = midend_get_presets(me, &idl);
    fill_presets();
    list_page = 0;
    show_list();
}

static void open_lang(void)
{
    list_begin(LK_LANG, T(S_LANGUAGE));
    list_add(LG_SYSTEM, T(S_SYSTEM), tr_setting() == LANG_SYSTEM, 0);
    list_add(LG_EN, "English", tr_setting() == LANG_EN, 0);
    list_add(LG_RU, "Русский", tr_setting() == LANG_RU, 0);
    list_add(LG_DE, "Deutsch", tr_setting() == LANG_DE, 0);
    list_page = 0;
    show_list();
}

static void open_confirm(void)
{
    list_begin(LK_CONFIRM, tr_game_name(cur->name));
    list_add(0, T(S_NEW_Q), 0, 0);
    show_list();
}

/* the n-th choice of a ":a:b:c" style list */
static void choice_name(const char *names, int n, char *out, size_t sz)
{
    char d = names[0];
    const char *p = names + 1;
    int i;
    for (i = 0; i < n; i++) {
        p = strchr(p, d);
        if (!p) { out[0] = 0; return; }
        p++;
    }
    {
        const char *e = strchr(p, d);
        size_t l = e ? (size_t)(e - p) : strlen(p);
        if (l >= sz) l = sz - 1;
        memcpy(out, p, l);
        out[l] = 0;
    }
}

static int choice_count(const char *names)
{
    char d = names[0];
    int n = 1;
    const char *p = names + 1;
    while ((p = strchr(p, d)) != NULL) { n++; p++; }
    return n;
}

static void fill_config(void)
{
    int i;
    char lab[120], val[240], line[400], ch[120], tr1[240];
    list_begin(LK_CONFIG, T(S_CUSTOM_TITLE));
    for (i = 0; cfg_items[i].type != C_END; i++) {
        config_item *c = &cfg_items[i];
        tr_glossary(c->name, lab, sizeof lab);
        switch (c->type) {
          case C_STRING: snprintf(val, sizeof val, "%s", c->u.string.sval); break;
          case C_BOOLEAN: snprintf(val, sizeof val, "%s", c->u.boolean.bval ? T(S_YES) : T(S_NO)); break;
          default:
            choice_name(c->u.choices.choicenames, c->u.choices.selected, ch, sizeof ch);
            tr_glossary(ch, tr1, sizeof tr1);
            snprintf(val, sizeof val, "%s", tr1);
            break;
        }
        snprintf(line, sizeof line, "%s: %s", lab, val);
        list_add(i, line, 0, 0);
    }
}

static void open_config(void)
{
    cfg_items = midend_get_config(me, CFG_SETTINGS, &cfg_title);
    fill_config();
    list_page = 0;
    show_list();
}

static void close_config(void)
{
    if (cfg_items) { free_cfg(cfg_items); cfg_items = NULL; }
    sfree(cfg_title);
    cfg_title = NULL;
}

static void kb_done(char *text)
{
    config_item *c;
    if (!text || !cfg_items) { paint(0); return; }
    c = &cfg_items[cfg_kb_item];
    sfree(c->u.string.sval);
    c->u.string.sval = dupstr(text);
    fill_config();
    paint(0);
}

static void open_rules(void)
{
    screen = SC_RULES;
}

/* ---- actions ---- */

static void back_from_list(void)
{
    if (list_kind == LK_CONFIG) close_config();
    screen = back_screen;
    paint(1);
}

static void do_solve(void)
{
    const char *err = midend_solve(me);
    message[0] = 0;
    if (err) snprintf(message, sizeof message, "%s", err);
    {
        int ch;
        fe_drain_timers(me);
        sync_flags(&ch);
        save_game();
    }
}

static void list_pick(int id)
{
    switch (list_kind) {
      case LK_MENU:
      case LK_HOME_MENU:
        switch (id) {
          case M_NEW: if (can_undo && !solved) { open_confirm(); paint(0); } else new_game(); return;
          case M_RESTART: midend_restart_game(me); setup_play(); screen = SC_PLAY; paint_play(); refresh(1); save_game(); return;
          case M_SOLVE: screen = SC_PLAY; do_solve(); paint_play(); refresh(1); return;
          case M_TYPE: open_presets(); paint(0); return;
          case M_RULES: open_rules(); paint(0); return;
          case M_LANG: open_lang(); paint(0); return;
          case M_THICK:
            thick = !thick;
            fe_set_thick(thick);
            cfg_set("thick", thick ? "1" : "0");
            if (list_kind == LK_MENU) open_game_menu(); else open_home_menu();
            paint(0);
            return;
          case M_CLEAN: paint(1); return;
          case M_EXPORT: case M_IMPORT: {
            char text[400];
            int n = id == M_EXPORT ? savezip_export() : savezip_import();
            if (n >= 0) snprintf(text, sizeof text, id == M_EXPORT ? T(S_EXPORTED) : T(S_IMPORTED), n, TAT_ZIP);
            else if (n == -2) snprintf(text, sizeof text, "%s", T(S_IMPORT_COMPRESSED));
            else snprintf(text, sizeof text, id == M_EXPORT ? T(S_EXPORT_FAIL) : T(S_IMPORT_NONE), TAT_ZIP);
            Message(n >= 0 ? ICON_INFORMATION : ICON_ERROR, T(id == M_EXPORT ? S_EXPORT : S_IMPORT), text, 6000);
            screen = SC_HOME;
            paint(1);
            return;
          }
          case M_HOME: go_home(); paint(1); return;
          case M_EXIT: close_game(); CloseApp(); return;
        }
        break;
      case LK_PRESETS: {
        struct preset_menu *m = pm_stack[pm_depth];
        if (id == M_CUSTOM) { open_config(); paint(0); return; }
        if (id >= 0 && id < m->n_entries) {
            struct preset_menu_entry *e = &m->entries[id];
            if (e->submenu) {
                if (pm_depth < 3) pm_stack[++pm_depth] = e->submenu;
                fill_presets();
                list_page = 0;
                paint(0);
            } else {
                midend_set_params(me, e->params);
                screen = SC_PLAY;
                new_game();
            }
        }
        return;
      }
      case LK_LANG:
        tr_set_setting(id == LG_SYSTEM ? LANG_SYSTEM : id == LG_EN ? LANG_EN : id == LG_RU ? LANG_RU : LANG_DE);
        back_from_list();
        return;
      case LK_CONFIG: {
        config_item *c = &cfg_items[id];
        if (c->type == C_BOOLEAN) { c->u.boolean.bval = !c->u.boolean.bval; fill_config(); paint(0); }
        else if (c->type == C_CHOICES) {
            c->u.choices.selected = (c->u.choices.selected + 1) % choice_count(c->u.choices.choicenames);
            fill_config();
            paint(0);
        } else {
            cfg_kb_item = id;
            snprintf(kbbuf, sizeof kbbuf, "%s", c->u.string.sval);
            OpenKeyboard(c->name, kbbuf, sizeof kbbuf - 1, KBD_NORMAL, kb_done);
        }
        return;
      }
    }
}

static void do_button(int id)
{
    int ch;
    if (id >= B_GAME && id < B_GAME + 100) { open_game(id - B_GAME); return; }
    if (id >= B_KEY && id < B_KEY + 64) {
        if (screen == SC_PLAY && me && id - B_KEY < nkeys) {
            int r = midend_process_key(me, -1, -1, keys[id - B_KEY].button);
            fe_drain_timers(me);
            sync_flags(&ch);
            if (r == PKR_SOME_EFFECT) save_game();
            if (ch) { draw_bottom(); draw_status(); PartialUpdate(0, H - bottom_h, W, bottom_h); }
        }
        return;
    }
    if (id >= B_ITEM && id < B_ITEM + MAXITEM) {
        int rows, top = sc(110), foot = sc(130), rowh = sc(108), first, idx;
        rows = (H - top - foot) / rowh;
        if (rows < 1) rows = 1;
        first = list_page * rows;
        idx = first + (id - B_ITEM);
        if (idx < nitems) list_pick(items[idx].id);
        return;
    }
    switch (id) {
      case B_GAMES: go_home(); paint(1); break;
      case B_NEW:
        if (can_undo && !solved) { open_confirm(); paint(0); } else new_game();
        break;
      case B_UNDO: case B_REDO:
        if (screen == SC_PLAY && me) {
            midend_process_key(me, -1, -1, id == B_UNDO ? UI_UNDO : UI_REDO);
            fe_drain_timers(me);
            sync_flags(&ch);
            save_game();
            message[0] = 0;
            draw_bottom(); draw_status();
            PartialUpdate(0, H - bottom_h, W, bottom_h);
            PartialUpdate(0, top_h, W, status_h);
        }
        break;
      case B_MODE:
        rmode = !rmode;
        draw_bottom();
        PartialUpdate(0, H - bottom_h, W, bottom_h);
        break;
      case B_MENU:
        if (screen == SC_HOME) open_home_menu(); else open_game_menu();
        paint(0);
        break;
      case B_PREV:
        if (screen == SC_HOME) home_page--; else list_page--;
        paint(0);
        break;
      case B_NEXT:
        if (screen == SC_HOME) home_page++; else list_page++;
        paint(0);
        break;
      case B_BACK:
        if (list_kind == LK_PRESETS && screen == SC_LIST && pm_depth > 0) {
            pm_depth--; fill_presets(); list_page = 0; paint(0);
        } else if (screen == SC_RULES) { screen = SC_PLAY; paint(1); }
        else back_from_list();
        break;
      case B_OK: {
        const char *err = midend_set_config(me, CFG_SETTINGS, cfg_items);
        if (err) { snprintf(message, sizeof message, "%s", err); fill_config(); paint(0); break; }
        close_config();
        screen = SC_PLAY;
        new_game();
        break;
      }
      case B_CANCEL: close_config(); screen = back_screen; paint(1); break;
      case B_YES: screen = SC_PLAY; new_game(); break;
      case B_NO: screen = SC_PLAY; paint(1); break;
    }
}

static int hit(int x, int y)
{
    int i;
    for (i = nbtn - 1; i >= 0; i--)
        if (x >= btns[i].x && x < btns[i].x + btns[i].w && y >= btns[i].y && y < btns[i].y + btns[i].h) return btns[i].id;
    return B_NONE;
}

/* ---- the board: tap, drag, hold for the right button ---- */

static int in_area(int x, int y)
{
    return x >= area.x && x < area.x + area.w && y >= area.y && y < area.y + area.h;
}

static int board_key(int button, int x, int y)
{
    int r, ch;
    r = midend_process_key(me, x - board.x, y - board.y, button);
    fe_drain_timers(me);
    sync_flags(&ch);
    if (r == PKR_SOME_EFFECT) {
        save_game();
        if (message[0]) { message[0] = 0; ch = 1; }
        if (++board_updates >= 30) { board_updates = 0; paint(1); return r; }
    }
    if (ch) {
        draw_bottom();
        draw_status();
        PartialUpdate(0, H - bottom_h, W, bottom_h);
        PartialUpdate(0, top_h, W, status_h);
    }
    return r;
}

static void pointer_down(int x, int y)
{
    memset(&ptr, 0, sizeof ptr);
    ptr.on_board = screen == SC_PLAY && me && in_area(x, y);
    if (ptr.on_board) { ptr.state = PT_PENDING; ptr.dx = x; ptr.dy = y; }
    else ptr.btn_id = hit(x, y);
}

static void pointer_move(int x, int y)
{
    int dead = sc(14), b = rmode ? 1 : 0;
    if (!ptr.on_board) return;
    if (ptr.state == PT_PENDING) {
        int ddx = x - ptr.dx, ddy = y - ptr.dy;
        if (ddx * ddx + ddy * ddy < dead * dead) return;
        /* an engine that does not use the press must not get the drag either (Train Tracks indexes its
         * grid with the drag's start otherwise) */
        ptr.state = board_key(b ? RIGHT_BUTTON : LEFT_BUTTON, ptr.dx, ptr.dy) == PKR_UNUSED ? PT_IGNORE : PT_DRAG;
    }
    if (ptr.state == PT_DRAG) board_key(b ? RIGHT_DRAG : LEFT_DRAG, x, y);
}

static void pointer_long(int x, int y)
{
    (void)x; (void)y;
    if (ptr.on_board && ptr.state == PT_PENDING)
        ptr.state = board_key(RIGHT_BUTTON, ptr.dx, ptr.dy) == PKR_UNUSED ? PT_IGNORE : PT_RIGHT;
}

static void pointer_up(int x, int y)
{
    int b = rmode ? 1 : 0;
    if (ptr.on_board) {
        if (ptr.state == PT_PENDING) {
            board_key(b ? RIGHT_BUTTON : LEFT_BUTTON, x, y);
            board_key(b ? RIGHT_RELEASE : LEFT_RELEASE, x, y);
        } else if (ptr.state == PT_DRAG) board_key(b ? RIGHT_RELEASE : LEFT_RELEASE, x, y);
        else if (ptr.state == PT_RIGHT) board_key(RIGHT_RELEASE, ptr.dx, ptr.dy);
    } else if (ptr.btn_id != B_NONE && hit(x, y) == ptr.btn_id) {
        do_button(ptr.btn_id);
    }
    memset(&ptr, 0, sizeof ptr);
}

/* ---- hardware keys ---- */

static void key_pressed(int k)
{
    switch (k) {
      case IV_KEY_BACK:
        if (screen == SC_PLAY) do_button(B_GAMES);
        else if (screen == SC_LIST || screen == SC_RULES) do_button(B_BACK);
        else { close_game(); CloseApp(); }
        break;
      case IV_KEY_MENU:
        if (screen == SC_PLAY || screen == SC_HOME) do_button(B_MENU);
        break;
      case IV_KEY_PREV:
        if (screen == SC_PLAY) do_button(B_UNDO);
        else if (screen == SC_HOME && home_page > 0) do_button(B_PREV);
        break;
      case IV_KEY_NEXT:
        if (screen == SC_PLAY) do_button(B_REDO);
        else if (screen == SC_HOME) do_button(B_NEXT);
        break;
    }
}

/* ---- InkView entry ---- */

static void start(void)
{
    char v[64];
    mkdir(TAT_DIR, 0777);
    tr_load();
    thick = cfg_get("thick", v, sizeof v) && v[0] == '1';
    fe_set_thick(thick);
}

static int main_handler(int type, int par1, int par2)
{
    switch (type) {
      case EVT_INIT:
        SetPanelType(PANEL_DISABLED);
        W = ScreenWidth(); H = ScreenHeight();
        fe_init();
        init_fonts();
        start();
        break;
      case EVT_SHOW:
        W = ScreenWidth(); H = ScreenHeight();
        paint(1);
        break;
      case EVT_ORIENTATION:
        SetOrientation(par1);
        W = ScreenWidth(); H = ScreenHeight();
        init_fonts();
        if (me) { layout(); fe_layout(me, area.x, area.y, area.w, area.h, &board); }
        paint(1);
        break;
      case EVT_POINTERDOWN: pointer_down(par1, par2); break;
      case EVT_POINTERMOVE:
      case EVT_POINTERDRAG: pointer_move(par1, par2); break;
      case EVT_POINTERLONG: pointer_long(par1, par2); break;
      case EVT_POINTERUP: pointer_up(par1, par2); break;
      case EVT_KEYPRESS: key_pressed(par1); break;
      case EVT_EXIT:
        close_game();
        close_config();
        fe_free();
        break;
      default: break;
    }
    return 0;
}

int main(void)
{
    InkViewMain(main_handler);
    return 0;
}
