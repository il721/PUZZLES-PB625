/* Puzzle games ("Playground"): a Puzzle Book module for the PocketBook 625 holding seven small games.
 * Game rules: pg_core.c (port of il721/puzzles, module_playground).
 * Eight chips, Cats and dogs, Hourglass: tap a piece (a ring marks it), then one of the marked places.
 * Three of each, Patterns 5x5 / 4x4: tap a tile in the tray (tap again or Rotate to turn it), then an empty place; a laid tile is
 *   turned in place with Rotate (or a second tap) and goes back to the tray with To tray.
 * Swap the squares: tap a block, dots mark where it can go; the first tap on a dot previews the landing place, the second moves.
 * Every applied move is recorded: Undo replays the list, the save file /mnt/ext1/playground.sav keeps it. Reset starts the game
 * over but keeps "solved" and the best move count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shell.h"
#include "pg_core.h"

#ifndef PLAYGROUNDSAVEF
#define PLAYGROUNDSAVEF "/mnt/ext1/playground.sav"
#endif

#define MAXMOVES 1500

enum { B_UNDO = 10, B_PREV, B_NEXT, B_RESET, B_LIST, B_HELP, B_HOME, B_CLOSE, B_ROT, B_TRAY };
enum { M_PLAY, M_LIST, M_HELP };

typedef UiBtn Btn;

static PgState st[PG_GAMES];
static PgMove stack[PG_GAMES][MAXMOVES];
static int nmoves[PG_GAMES];
static int solved[PG_GAMES], best[PG_GAMES];
static long long stamp[PG_GAMES]; /* ms since 1970 of the last change to each game, 0 = unknown */
static int cur, mode = M_PLAY;
static int reset_armed, updates;
static char msg[200];
static int left; /* set when the module handed control back to the shell */

/* selection (not saved) */
static int sel_node = -1;
static unsigned char node_dest[PG_MAXN];
static int sel_tray = -1, sel_rot, sel_slot = -1;
static int sel_block = -1, prev_org = -1;
static unsigned char block_dest[PG_SW_CELLS];

static Btn btns[16];
static int nbtn;
static int title_x, title_w;
static int bx0, by0, bw0, bh0;          /* the area the board may use */
static int st_x, st_y, st_w, st_h;      /* status text */
/* graph games */
static int nx[PG_MAXN], ny[PG_MAXN], nr;
/* tile games */
static int tbx, tby, tcs, tile_cells, board_side;           /* board origin, cell size, cells per tile, tiles per row */
static int tray_x, tray_y, tray_cols, tray_tile, tray_gap, tray_cs;
/* swap blocks */
static int sbx, sby, scs;

static ifont *f_node, *f_mini;
static int f_node_size;
/* the goal picture of Eight chips (bottom left, the status text to its right) */
static int gp_on, gp_x, gp_y, gp_sz;

/* ---------- texts (English, Russian, German; i18n.h) ---------- */

static const Str S_LIST = S("List", "Список", "Liste");
static const Str S_HOME = S("Home", "Домой", "Start");
static const Str S_RESET = S("Reset", "Сбросить", "Reset");
static const Str S_UNDO = S("Undo", "Отмена", "Rückgängig");
static const Str S_ROTATE = S("Rotate", "Повернуть", "Drehen");
static const Str S_TOTRAY = S("To tray", "В набор", "In Vorrat");
static const Str S_BACK = S("Back", "Назад", "Zurück");
static const Str S_TITLE = S("%s  %d/%d%s", "%s  %d/%d%s", "%s  %d/%d%s");
static const Str S_LIST_TITLE = S("Puzzle games - choose one", "Выберите игру", "Spiel wählen");
static const Str S_TAP_CLOSE = S("Tap anywhere to close.", "Нажмите в любом месте, чтобы закрыть.", "Tippen Sie irgendwo, um zu schließen.");
static const Str S_MOVES = S("Moves: %d", "Ходов: %d", "Züge: %d");
static const Str S_BEST = S("Best: %d", "Рекорд: %d", "Bestwert: %d");
static const Str S_MIN = S("Minimum: %d moves", "Минимум: %d ходов", "Minimum: %d Züge");
static const Str S_PLACED = S("Tiles laid: %d of %d", "Выложено: %d из %d", "Gelegt: %d von %d");
static const Str S_SOLVED_N = S("Solved in %d moves!", "Решено за %d ходов!", "Gelöst in %d Zügen!");
static const Str S_SOLVED = S("Solved!", "Решено!", "Gelöst!");
static const Str S_OPTIMAL = S("That is the minimum!", "Это минимум!", "Das ist das Minimum!");
static const Str S_NEXT = S("Press > for the next game.", "Нажмите > для следующей игры.", "Mit > geht es zum nächsten Spiel.");
static const Str S_RESET_AGAIN = S("Tap Reset again to start this game over.", "Нажмите «Сбросить» ещё раз, чтобы начать игру заново.",
                                   "Noch einmal „Reset“ tippen, um das Spiel neu zu beginnen.");
static const Str S_HINT_GRAPH = S("Tap a piece, then a marked place.", "Нажмите фишку, затем отмеченное место.", "Stein antippen, dann einen markierten Platz.");
static const Str S_HINT_DEST = S("Tap a marked place (the piece again lets go).", "Нажмите отмеченное место (повторно фишку — отмена).",
                                 "Markierten Platz antippen (Stein erneut: loslassen).");
static const Str S_HINT_TRAY = S("Tap a tile in the tray, then an empty place.", "Нажмите квадратик в наборе, затем пустое место.",
                                 "Plättchen im Vorrat antippen, dann einen leeren Platz.");
static const Str S_HINT_PLACE = S("Tap an empty place (tap the tile again to turn it).", "Нажмите пустое место (повторное нажатие поворачивает квадратик).",
                                  "Leeren Platz antippen (noch einmal antippen dreht das Plättchen).");
static const Str S_HINT_LAID = S("Rotate turns this tile, To tray takes it back.", "«Повернуть» поворачивает квадратик, «В набор» убирает его.",
                                 "„Drehen“ dreht das Plättchen, „In Vorrat“ nimmt es zurück.");
static const Str S_HINT_BLOCK = S("Tap a block, then a dot (twice to move).", "Нажмите блок, затем точку (дважды — переезд).", "Block antippen, dann einen Punkt (zweimal: fahren).");
static const Str S_HINT_DOT = S("Tap a dot to preview, tap it again to move.", "Нажмите точку — предпросмотр, ещё раз — переезд.", "Punkt antippen: Vorschau, noch einmal: fahren.");
static const Str S_HINT_MOVE = S("Tap the dot again to move the block there.", "Нажмите точку ещё раз, чтобы переместить блок.", "Punkt noch einmal antippen, um den Block dorthin zu fahren.");
static const Str S_NO_MOVES = S("This piece cannot move now.", "Эта фишка сейчас не может ходить.", "Dieser Stein kann jetzt nicht ziehen.");
static const Str S_NO_MOVES_B = S("This block cannot move now.", "Этот блок сейчас не может двигаться.", "Dieser Block kann jetzt nicht fahren.");
static const Str S_PICK_FIRST = S("Select a tile first.", "Сначала выберите квадратик.", "Wählen Sie zuerst ein Plättchen.");
static const Str S_LOOSE = S("A line end meets an empty edge or leaves the square.", "Конец линии упирается в пустой край или выходит за квадрат.",
                             "Ein Linienende trifft auf einen leeren Rand oder verlässt das Quadrat.");
static const Str S_LOOPS = S("The line closes into several loops.", "Линия замкнулась в несколько петель.", "Die Linie schließt sich zu mehreren Schleifen.");
static const Str S_LINES = S("Some row, column or diagonal is not right yet.", "Какая-то строка, столбец или диагональ ещё не готова.",
                             "Eine Zeile, Spalte oder Diagonale stimmt noch nicht.");
static const Str S_FULL = S("Move limit reached - press Reset.", "Предел ходов — нажмите «Сбросить».", "Zuglimit erreicht - „Reset“ tippen.");
static const Str S_GOAL = S("Goal", "Цель", "Ziel");
static const Str S_LS_NEW = S("new", "новая", "neu");
static const Str S_LS_PLAY = S("in progress", "в процессе", "begonnen");
static const Str S_LS_DONE = S("solved", "решена", "gelöst");
static const Str S_LS_BEST = S("solved, best %d", "решена, рекорд %d", "gelöst, Bestwert %d");

static const Str S_GAME[PG_GAMES] = {
    S("Eight chips", "Восемь фишек", "Acht Chips"),
    S("Cats and dogs", "Кошки и собаки", "Katzen und Hunde"),
    S("Hourglass", "Песочные часы", "Sanduhr"),
    S("Three of each", "Всюду по три", "Überall drei"),
    S("Patterns 5×5", "Узоры 5×5", "Muster 5×5"),
    S("Patterns 4×4", "Узоры 4×4", "Muster 4×4"),
    S("Swap the squares", "Поменяйте местами", "Tausche die Quadrate")};

static const Str S_RULES[PG_GAMES] = {
    S("Pieces 1-8 stand at the tips of an eight-pointed star. A piece slides along a straight line to a free spot (through the free centre it "
      "may go on along the same line). Arrange the pieces in reverse order: piece 1 where 8 stood, 2 where 7 stood, and so on, with the "
      "centre free. The minimum is 28 moves.\n\nTap a piece, then a marked spot. Tap the piece again to let go of it.",
      "Фишки 1–8 стоят на концах восьмиконечной звезды. Фишка скользит по прямой линии на свободное место (через свободный центр она "
      "может проехать дальше по той же линии). Расставьте фишки в обратном порядке: 1 — на место 8, 2 — на место 7 и так далее, "
      "центр свободен. Минимум — 28 ходов.\n\nНажмите фишку, затем отмеченное место. Повторное нажатие на фишку снимает выбор.",
      "Die Chips 1–8 stehen an den Spitzen eines achtzackigen Sterns. Ein Chip gleitet entlang einer Linie auf einen freien Platz (durch die "
      "freie Mitte darf er auf derselben Linie weiterrutschen). Ordnen Sie die Chips in umgekehrter Reihenfolge: 1 dorthin, wo 8 stand, "
      "2 dorthin, wo 7 stand, usw.; die Mitte bleibt frei. Das Minimum sind 28 Züge.\n\nChip antippen, dann einen markierten Platz. "
      "Den Chip noch einmal antippen hebt die Wahl auf."),
    S("Three cats (C) sit on the left squares of the park, three dogs (D) on the right. Each move takes one animal along an alley to a "
      "neighbouring free square. A cat and a dog may never stand on neighbouring squares: such moves are not offered. Make cats and dogs "
      "swap sides. The minimum is 32 moves.\n\nTap an animal, then a marked square.",
      "Три кошки (C) сидят на левых площадках сквера, три собаки (D) — на правых. За ход одно животное перебегает по аллее на соседнюю "
      "свободную площадку. Кошка и собака не могут стоять на соседних площадках — такие ходы не предлагаются. Поменяйте кошек и собак "
      "местами. Минимум — 32 хода.\n\nНажмите животное, затем отмеченную площадку.",
      "Drei Katzen (C) sitzen auf den linken Plätzen des Parks, drei Hunde (D) auf den rechten. Pro Zug läuft ein Tier über eine Allee auf "
      "einen benachbarten freien Platz. Katze und Hund dürfen nie auf Nachbarplätzen stehen: solche Züge werden nicht angeboten. Katzen "
      "und Hunde sollen die Seiten tauschen. Das Minimum sind 32 Züge.\n\nTier antippen, dann einen markierten Platz."),
    S("Fifteen pieces fill the upper triangle of an hourglass; bring all of them into the lower one. A move steps a piece along a line onto a "
      "neighbouring free circle, or jumps it, as in draughts, over a neighbouring piece onto the free circle behind it. Several jumps in "
      "a row by one piece are one move and you may stop after any jump; a step may not be mixed with jumps. The minimum is 26 moves."
      "\n\nTap a piece, then a marked circle.",
      "Пятнадцать фишек заполняют верхний треугольник песочных часов; переведите все в нижний. За ход фишка переходит по линии на "
      "соседний свободный кружок или перепрыгивает, как в шашках, через соседнюю фишку на свободный кружок за ней. Несколько прыжков "
      "подряд одной фишкой — один ход, остановиться можно после любого прыжка; переход и прыжки смешивать нельзя. Минимум — 26 ходов."
      "\n\nНажмите фишку, затем отмеченный кружок.",
      "Fünfzehn Steine füllen das obere Dreieck einer Sanduhr; bringen Sie alle in das untere. Pro Zug rückt ein Stein entlang einer Linie "
      "auf einen freien Nachbarkreis oder springt wie beim Damespiel über einen Nachbarstein auf den freien Kreis dahinter. Mehrere "
      "Sprünge nacheinander mit demselben Stein sind ein Zug, man darf nach jedem Sprung anhalten; Rücken und Springen nicht mischen. "
      "Das Minimum sind 26 Züge.\n\nStein antippen, dann einen markierten Kreis."),
    S("Nine 3x3 tiles carry red (grey), white (ring) and black (solid) circles. Lay them into a 9x9 square so that each of the nine rows, "
      "nine columns and both diagonals holds exactly one circle of each colour. Tiles turn but never flip; the number of moves does not "
      "matter.\n\nTap a tile in the tray, tap it again (or Rotate) to turn it, then tap an empty place. A laid tile turns in place "
      "(Rotate) or goes back (To tray).",
      "Девять квадратиков 3×3 расписаны красными (серые), белыми (кольца) и чёрными (сплошные) кружками. Сложите из них квадрат 9×9 так, "
      "чтобы на каждой из девяти строк, девяти столбцов и обеих диагоналей оказалось ровно по одному кружку каждого цвета. Квадратики "
      "поворачивают, но не переворачивают; число ходов не важно.\n\nНажмите квадратик в наборе, нажмите ещё раз (или «Повернуть»), "
      "чтобы повернуть, затем нажмите пустое место. Выложенный квадратик можно повернуть на месте («Повернуть») или вернуть («В набор»).",
      "Neun 3×3-Plättchen tragen rote (grau), weiße (Ring) und schwarze (voll) Kreise. Legen Sie sie zu einem 9×9-Quadrat, sodass jede der "
      "neun Zeilen, neun Spalten und beide Diagonalen genau einen Kreis jeder Farbe enthält. Plättchen werden gedreht, nie gewendet; die "
      "Zugzahl spielt keine Rolle.\n\nPlättchen im Vorrat antippen, noch einmal antippen (oder „Drehen“), um es zu drehen, dann einen "
      "leeren Platz antippen. Ein gelegtes Plättchen lässt sich an Ort und Stelle drehen („Drehen“) oder zurücklegen („In Vorrat“)."),
    S("Twenty-five tiles carry line pieces. Lay them into a 5x5 square so that all pieces join into one closed line that never crosses "
      "itself. The line runs through every tile, so only the joins can go wrong: an end meeting an empty edge, an end pointing off the "
      "square, or several separate loops. Tiles turn but never flip; the number of moves does not matter.\n\nTap a tile in the tray, tap it "
      "again (or Rotate) to turn it, then tap an empty place. A laid tile turns in place (Rotate) or goes back (To tray).",
      "Двадцать пять квадратиков расчерчены линиями. Сложите из них квадрат 5×5 так, чтобы отрезки сомкнулись в одну замкнутую линию без "
      "самопересечений. Линия проходит по всем квадратикам, поэтому ошибиться можно только на стыках: конец упирается в пустой край, "
      "торчит за границу квадрата или отрезки замыкаются в несколько петель. Квадратики поворачивают, но не переворачивают; число ходов "
      "не важно.\n\nНажмите квадратик в наборе, нажмите ещё раз (или «Повернуть»), затем нажмите пустое место. Выложенный квадратик "
      "можно повернуть на месте или вернуть («В набор»).",
      "Fünfundzwanzig Plättchen tragen Linienstücke. Legen Sie sie zu einem 5×5-Quadrat, sodass sich alle Stücke zu einer einzigen "
      "geschlossenen, sich nirgends kreuzenden Linie fügen. Die Linie läuft durch jedes Plättchen, schiefgehen können nur die Übergänge: "
      "ein Ende trifft auf einen leeren Rand, zeigt aus dem Quadrat heraus, oder es entstehen mehrere Schleifen. Plättchen werden gedreht, "
      "nie gewendet; die Zugzahl spielt keine Rolle.\n\nPlättchen im Vorrat antippen, noch einmal antippen (oder „Drehen“), dann einen "
      "leeren Platz antippen. Ein gelegtes Plättchen lässt sich an Ort und Stelle drehen oder zurücklegen („In Vorrat“)."),
    S("Sixteen tiles carry line pieces - exactly those the book's own answer uses. Lay them into a 4x4 square so that all pieces join into "
      "one closed line that never crosses itself. The line runs through every tile, so only the joins can go wrong: an end meeting an empty "
      "edge, an end pointing off the square, or several separate loops. Tiles turn but never flip; the number of moves does not matter."
      "\n\nTap a tile in the tray, tap it again (or Rotate) to turn it, then tap an empty place. A laid tile turns in place (Rotate) or "
      "goes back (To tray).",
      "Шестнадцать квадратиков расчерчены линиями — ровно те, которыми сложен ответ в книге. Сложите из них квадрат 4×4 так, чтобы "
      "отрезки сомкнулись в одну замкнутую линию без самопересечений. Линия проходит по всем квадратикам, поэтому ошибиться можно только "
      "на стыках: конец упирается в пустой край, торчит за границу квадрата или отрезки замыкаются в несколько петель. Квадратики "
      "поворачивают, но не переворачивают; число ходов не важно.\n\nНажмите квадратик в наборе, нажмите ещё раз (или «Повернуть»), затем "
      "нажмите пустое место. Выложенный квадратик можно повернуть на месте или вернуть («В набор»).",
      "Sechzehn Plättchen tragen Linienstücke - genau die, die auch die Lösung im Buch verwendet. Legen Sie sie zu einem 4×4-Quadrat, "
      "sodass sich alle Stücke zu einer einzigen geschlossenen, sich nirgends kreuzenden Linie fügen. Die Linie läuft durch jedes "
      "Plättchen, schiefgehen können nur die Übergänge: ein Ende trifft auf einen leeren Rand, zeigt aus dem Quadrat heraus, oder es "
      "entstehen mehrere Schleifen. Plättchen werden gedreht, nie gewendet; die Zugzahl spielt keine Rolle.\n\nPlättchen im Vorrat "
      "antippen, noch einmal antippen (oder „Drehen“), dann einen leeren Platz antippen. Ein gelegtes Plättchen lässt sich an Ort und "
      "Stelle drehen oder zurücklegen („In Vorrat“)."),
    S("Twelve P-shaped blocks fill a 16x8 box and leave 32 cells free. A move slides one block through the free space, sideways and up or "
      "down, any distance and round corners; the whole trip is one move. Blocks never turn. Swap the two grey blocks (top) with the two "
      "black ones (bottom) while every other block returns to its place. The minimum is 32 moves.\n\nTap a block: dots mark where it can go. "
      "Tap a dot to preview the landing place, tap it again to move.",
      "Двенадцать одинаковых P-образных блоков заполняют ящик 16×8; 32 клетки свободны. За ход один блок скользит по свободному месту "
      "вбок и вверх-вниз на любое расстояние, поворачивая за углы; всё путешествие — один ход. Блоки не поворачиваются. Поменяйте "
      "местами два серых блока (сверху) и два чёрных (снизу), а все остальные блоки верните на свои места. Минимум — 32 хода."
      "\n\nНажмите блок: точки отмечают, куда он может уехать. Нажмите точку — покажется место, нажмите ещё раз — блок переедет.",
      "Zwölf P-förmige Blöcke füllen einen Kasten von 16×8 Feldern, 32 Felder bleiben frei. Pro Zug gleitet ein Block durch den freien "
      "Raum, seitwärts und auf oder ab, beliebig weit und um Ecken; die ganze Fahrt ist ein Zug. Blöcke werden nie gedreht. Tauschen Sie "
      "die zwei grauen Blöcke (oben) mit den zwei schwarzen (unten), alle anderen Blöcke kehren an ihren Platz zurück. Das Minimum "
      "sind 32 Züge.\n\nBlock antippen: Punkte zeigen, wohin er fahren kann. Punkt antippen zeigt die Vorschau, noch einmal antippen fährt.")};

/* ---------- persistence ---------- */

static char line_buf[MAXMOVES * 11 + 128];

static void save_state(void) {
  FILE *f = fopen(PLAYGROUNDSAVEF, "w");
  int g, i;
  if (!f) return;
  fprintf(f, "v1 %d\n", cur);
  for (g = 0; g < PG_GAMES; g++) {
    fprintf(f, "%d %d %lld %d", solved[g], best[g], stamp[g], nmoves[g]);
    for (i = 0; i < nmoves[g]; i++) fprintf(f, " %u", pg_pack(stack[g][i]));
    fputc('\n', f);
  }
  fclose(f);
}

static void load_state(void) {
  FILE *f = fopen(PLAYGROUNDSAVEF, "r");
  int g;
  if (!f) return;
  /* atoi / strtol instead of sscanf: sscanf pulls in GLIBC_2.7, the firmware's libc may be older */
  if (fgets(line_buf, sizeof line_buf, f) && strncmp(line_buf, "v1 ", 3) == 0) {
    int v = atoi(line_buf + 3);
    if (v >= 0 && v < PG_GAMES) cur = v;
  }
  for (g = 0; g < PG_GAMES && fgets(line_buf, sizeof line_buf, f); g++) {
    char *p = line_buf, *e;
    int n, i;
    solved[g] = strtol(p, &e, 10) ? 1 : 0;
    p = e;
    best[g] = (int)strtol(p, &e, 10);
    if (best[g] < 0) best[g] = 0;
    p = e;
    stamp[g] = strtoll(p, &e, 10);
    p = e;
    n = (int)strtol(p, &e, 10);
    p = e;
    if (n < 0 || n > MAXMOVES) n = 0;
    for (i = 0; i < n; i++) {
      unsigned long v = strtoul(p, &e, 10);
      if (e == p) break;
      stack[g][i] = pg_unpack((unsigned)v);
      p = e;
    }
    if (i == n && pg_replay(g, &st[g], stack[g], n) == n) nmoves[g] = n;
    else { nmoves[g] = 0; pg_init(g, &st[g]); } /* a damaged list: the board starts over, solved and best are kept */
  }
  fclose(f);
}

/* ---------- fonts and small drawing helpers ---------- */

static void ensure_node_font(int size) {
  if (size < 14) size = 14;
  if (f_node && size == f_node_size) return;
  if (f_node) CloseFont(f_node);
  f_node = ui_open_font("LiberationSans-Bold", size);
  f_node_size = size;
}

static void fill_circle(int cx, int cy, int r, int color) {
  int dy, dx = r;
  for (dy = 0; dy <= r; dy++) {
    while (dx > 0 && dx * dx + dy * dy > r * r + r) dx--;
    FillArea(cx - dx, cy - dy, 2 * dx + 1, 1, color);
    if (dy) FillArea(cx - dx, cy + dy, 2 * dx + 1, 1, color);
  }
}

static void ring_circle(int cx, int cy, int r, int th, int fill) {
  fill_circle(cx, cy, r, BLACK);
  fill_circle(cx, cy, r - th, fill);
}

static void frame(int x, int y, int w, int h, int th, int color) {
  int i;
  for (i = 0; i < th; i++) DrawRect(x + i, y + i, w - 2 * i, h - 2 * i, color);
}

static void thick_line(int x1, int y1, int x2, int y2, int t, int color) {
  int dx = x2 - x1, dy = y2 - y1, i;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  for (i = -(t / 2); i < t - t / 2; i++) {
    if (dx >= dy) DrawLine(x1, y1 + i, x2, y2 + i, color);
    else DrawLine(x1 + i, y1, x2 + i, y2, color);
  }
}

static void add_btn(int x, int y, int w, int h, int id, const char *label) {
  Btn *b = &btns[nbtn++];
  b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

static int is_graph(void) { return pg_kind(cur) == PGK_GRAPH; }
static int is_tiles(void) { return pg_kind(cur) == PGK_TILES; }

/* ---------- layout (portrait) ---------- */

/* tile games: the largest board cell size and then tray cell size such that the board and the tray fit */
static int tray_rows(int ntiles, int cols) { return (ntiles + cols - 1) / cols; }

static void layout_tiles_portrait(int ax, int ay, int aw, int ah) {
  int ncell = board_side * tile_cells, nt = pg_ntiles(cur), cs, ts, tg = 4, gap = 8;
  int max_ts = cur == PG_THREE ? 20 : 26, min_cs = cur == PG_THREE ? 40 : 32;
  for (ts = max_ts; ts >= 12; ts--) { /* the largest tray tiles that still leave the board a decent size */
    int tile = tile_cells * ts, cols = (aw + tg) / (tile + tg), rows, th;
    if (cols < 1) continue;
    rows = tray_rows(nt, cols);
    th = rows * (tile + tg) - tg;
    cs = (ah - gap - th) / ncell;
    if (cs > aw / ncell) cs = aw / ncell;
    if (cs > 80) cs = 80;
    if (cs >= min_cs || ts == 12) {
      if (cs < 12) cs = 12;
      tcs = cs; tray_cs = ts; tray_tile = tile; tray_gap = tg; tray_cols = cols;
      tbx = ax + (aw - ncell * cs) / 2;
      tby = ay;
      tray_x = ax + (aw - (cols * (tile + tg) - tg)) / 2;
      tray_y = ay + ncell * cs + gap + (ah - ncell * cs - gap - th) / 2;
      return;
    }
  }
}

static void layout(int W, int H) {
  int m = 8, gap = 6, tb = 58;
  int kind = pg_kind(cur), top = tb + 6;
  nbtn = 0;
  gp_on = 0;
  add_btn(m, m, 120, tb - m, B_LIST, tr(&S_LIST));
  add_btn(W - m - 90, m, 90, tb - m, B_HOME, tr(&S_HOME));
  add_btn(W - m - 90 - gap - 60, m, 60, tb - m, B_HELP, "?");
  title_x = m + 120 + gap;
  title_w = W - 2 * m - 120 - 90 - 60 - 3 * gap;
  if (kind == PGK_TILES) {
    board_side = pg_side(cur);
    tile_cells = cur == PG_THREE ? 3 : 2;
  }
  {
    int bh = 60, by = H - m - bh, w4 = (W - 2 * m - 3 * gap) / 4, ctrl_top = by, lh = ui_f_small->height;
    add_btn(m, by, w4, bh, B_PREV, "<");
    add_btn(m + w4 + gap, by, w4, bh, B_UNDO, tr(&S_UNDO));
    add_btn(m + 2 * (w4 + gap), by, w4, bh, B_RESET, tr(&S_RESET));
    add_btn(m + 3 * (w4 + gap), by, W - m - (m + 3 * (w4 + gap)), bh, B_NEXT, ">");
    if (kind == PGK_TILES) {
      int w2 = (W - 2 * m - gap) / 2, y2 = by - gap - 52;
      add_btn(m, y2, w2, 52, B_ROT, tr(&S_ROTATE));
      add_btn(m + w2 + gap, y2, W - m - (m + w2 + gap), 52, B_TRAY, tr(&S_TOTRAY));
      ctrl_top = y2;
    }
    st_h = lh * (kind == PGK_TILES ? 4 : 3) + 6;
    st_w = W - 2 * m;
    st_x = m;
    if (cur == PG_EIGHT) { /* the goal picture bottom left, the text to its right */
      gp_on = 1;
      gp_sz = 126;
      if (st_h < gp_sz) st_h = gp_sz;
      gp_x = m;
      gp_y = ctrl_top - gap - gp_sz;
      st_x = m + gp_sz + gap;
      st_w = W - m - st_x;
      st_h = gp_sz;
    }
    st_y = ctrl_top - gap - st_h;
    bx0 = m; by0 = top; bw0 = W - 2 * m; bh0 = st_y - gap - top;
    if (kind == PGK_TILES) layout_tiles_portrait(bx0, by0, bw0, bh0);
  }
  /* the board geometry of the current game inside (bx0, by0, bw0, bh0) */
  if (kind == PGK_GRAPH) {
    int sz = bw0 < bh0 ? bw0 : bh0, L = pg_lattice(cur), i, ox, oy, inset, span;
    if (cur == PG_EIGHT) nr = sz / 13;
    else if (cur == PG_CATS) nr = (sz - 8) * 46 / 492;
    else nr = (sz - 8) / 18;
    inset = nr + 4;
    span = sz - 2 * inset;
    ox = bx0 + (bw0 - sz) / 2;
    oy = by0 + (bh0 - sz) / 2;
    for (i = 0; i < pg_nnodes(cur); i++) {
      int x, y;
      pg_node_xy(cur, i, &x, &y);
      nx[i] = ox + inset + span * x / L;
      ny[i] = oy + inset + span * y / L;
    }
  } else if (kind == PGK_BLOCKS) {
    scs = bh0 / PG_SW_ROWS;
    if (scs > bw0 / PG_SW_COLS) scs = bw0 / PG_SW_COLS;
    if (scs > 60) scs = 60;
    sbx = bx0 + (bw0 - scs * PG_SW_COLS) / 2;
    sby = by0 + (bh0 - scs * PG_SW_ROWS) / 2;
  } /* tile games: layout_tiles_portrait already placed the board and the tray */
}

/* ---------- drawing: graph games ---------- */

static void draw_graph(const PgState *s) {
  int l, i, k;
  ensure_node_font(cur == PG_EIGHT ? nr * 12 / 10 : nr);
  for (l = 0; l < pg_nlines(cur); l++) {
    const unsigned char *nd;
    int len = pg_line(cur, l, &nd);
    for (k = 0; k + 1 < len; k++) thick_line(nx[nd[k]], ny[nd[k]], nx[nd[k + 1]], ny[nd[k + 1]], 3, 0x555555);
  }
  for (i = 0; i < pg_nnodes(cur); i++) {
    int tok = s->tok[i], dest = sel_node >= 0 && node_dest[i];
    char t[4] = {0, 0, 0, 0};
    if (!tok) {
      ring_circle(nx[i], ny[i], nr, dest ? 5 : 2, WHITE);
      if (dest) fill_circle(nx[i], ny[i], nr / 4 + 1, BLACK);
      continue;
    }
    if (i == sel_node) ring_circle(nx[i], ny[i], nr + 7, 3, WHITE);
    if (cur == PG_EIGHT) {
      ring_circle(nx[i], ny[i], nr, 3, i == sel_node ? BLACK : 0xDDDDDD);
      t[0] = (char)('0' + tok);
      ui_text_center(f_node, i == sel_node ? WHITE : BLACK, nx[i] - nr, ny[i] - nr, 2 * nr, 2 * nr, t);
    } else if (cur == PG_CATS) {
      if (tok == 1) { fill_circle(nx[i], ny[i], nr, BLACK); t[0] = 'C'; ui_text_center(f_node, WHITE, nx[i] - nr, ny[i] - nr, 2 * nr, 2 * nr, t); }
      else { ring_circle(nx[i], ny[i], nr, 3, 0xCCCCCC); t[0] = 'D'; ui_text_center(f_node, BLACK, nx[i] - nr, ny[i] - nr, 2 * nr, 2 * nr, t); }
    } else {
      fill_circle(nx[i], ny[i], nr, BLACK);
    }
  }
}

/* the goal of Eight chips as a small picture: chip t where chip 9-t stood, the centre free */
static void draw_goal(void) {
  int i, lab = ui_f_tiny->height + 2, box = gp_sz, cr = box / 11, size = box - lab - 2 * (cr + 3);
  int ox = gp_x + cr + 3, oy = gp_y + lab + cr + 3;
  char t[4] = {0, 0, 0, 0};
  {
    static int mini_size;
    if (!f_mini || mini_size != cr) {
      if (f_mini) CloseFont(f_mini);
      f_mini = ui_open_font("LiberationSans-Bold", cr * 12 / 10 < 12 ? 12 : cr * 12 / 10);
      mini_size = cr;
    }
  }
  FillArea(gp_x, gp_y, box, box, WHITE);
  frame(gp_x, gp_y, box, box, 2, BLACK);
  ui_text_center(ui_f_tiny, BLACK, gp_x, gp_y + 2, box, lab, tr(&S_GOAL));
  for (i = 0; i < 9; i++) {
    int x, y, cx, cy;
    pg_node_xy(PG_EIGHT, i, &x, &y);
    cx = ox + size * x / 1000;
    cy = oy + size * y / 1000;
    if (i == 8) { ring_circle(cx, cy, cr, 1, WHITE); continue; }
    ring_circle(cx, cy, cr, 2, 0xDDDDDD);
    t[0] = (char)('0' + 8 - i);
    ui_text_center(f_mini, BLACK, cx - cr, cy - cr, 2 * cr, 2 * cr, t);
  }
}

/* ---------- drawing: tile games ---------- */

static void draw_circle_symbol(char ch, int cx, int cy, int r) {
  if (ch == 'R') ring_circle(cx, cy, r, 2, 0x999999);
  else if (ch == 'W') ring_circle(cx, cy, r, r > 9 ? 3 : 2, WHITE);
  else if (ch == 'B') fill_circle(cx, cy, r, BLACK);
}

/* the art of one tile (three of each: circles; patterns: line pieces) with its top-left corner at (x, y), cell size cs */
static void draw_tile_art(int tile, int rot, int x, int y, int cs) {
  int r, c;
  if (cur == PG_THREE) {
    for (r = 0; r < 3; r++)
      for (c = 0; c < 3; c++) draw_circle_symbol(pg_three_cell(tile, rot, r, c), x + c * cs + cs / 2, y + r * cs + cs / 2, cs * 36 / 100);
  } else {
    int t = 3 + cs / 16;
    for (r = 0; r < 2; r++)
      for (c = 0; c < 2; c++) {
        int m = pg_pat_mask(cur, tile, rot, r * 2 + c), cx = x + c * cs + cs / 2, cy = y + r * cs + cs / 2, h = cs / 2;
        if (m & PG_N) FillArea(cx - t / 2, cy - h, t, h + t / 2 + 1, BLACK);
        if (m & PG_S) FillArea(cx - t / 2, cy - t / 2, t, h + t / 2 + 1, BLACK);
        if (m & PG_W) FillArea(cx - h, cy - t / 2, h + t / 2 + 1, t, BLACK);
        if (m & PG_E) FillArea(cx - t / 2, cy - t / 2, h + t / 2 + 1, t, BLACK);
      }
  }
}

static void draw_tile_cells(int x, int y, int cs, int fill) {
  int k, n = tile_cells;
  FillArea(x, y, n * cs, n * cs, fill);
  for (k = 1; k < n; k++) {
    DrawLine(x + k * cs, y, x + k * cs, y + n * cs, 0xBBBBBB);
    DrawLine(x, y + k * cs, x + n * cs, y + k * cs, 0xBBBBBB);
  }
}

static void slot_origin(int slot, int *x, int *y) {
  *x = tbx + (slot % board_side) * tile_cells * tcs;
  *y = tby + (slot / board_side) * tile_cells * tcs;
}

static void tray_origin(int tile, int *x, int *y) {
  *x = tray_x + (tile % tray_cols) * (tray_tile + tray_gap);
  *y = tray_y + (tile / tray_cols) * (tray_tile + tray_gap);
}

static void draw_tiles(const PgState *s) {
  int slot, t, x, y, px = tile_cells * tcs, ncell = board_side * tile_cells;
  for (slot = 0; slot < pg_nslots(cur); slot++) {
    int sel = slot == sel_slot, target = sel_tray >= 0 && s->tile[slot] < 0;
    slot_origin(slot, &x, &y);
    draw_tile_cells(x, y, tcs, sel ? 0xDDDDDD : target ? 0xE4E4E4 : WHITE);
    if (s->tile[slot] >= 0) draw_tile_art(s->tile[slot], s->rot[slot], x, y, tcs);
    frame(x, y, px + 1, px + 1, sel ? 5 : 2, BLACK);
  }
  frame(tbx - 2, tby - 2, ncell * tcs + 5, ncell * tcs + 5, 3, BLACK);
  for (t = 0; t < pg_ntiles(cur); t++) {
    int pl = pg_tile_slot(cur, s, t) >= 0, sel = t == sel_tray, tp = tray_tile + 1;
    tray_origin(t, &x, &y);
    if (pl) {
      FillArea(x, y, tray_tile, tray_tile, 0xEEEEEE);
      DrawRect(x, y, tp, tp, 0xAAAAAA);
      continue;
    }
    draw_tile_cells(x, y, tray_cs, sel ? 0xDDDDDD : WHITE);
    draw_tile_art(t, sel ? sel_rot : 0, x, y, tray_cs);
    frame(x, y, tp, tp, sel ? 4 : 2, BLACK);
  }
}

/* ---------- drawing: swap blocks ---------- */

static int block_fill(int b) {
  int c = pg_swap_colour(b);
  return c == 1 ? 0x999999 : c == 2 ? 0x222222 : 0xE6E6E6;
}

/* the outline of a block (or of its preview) whose cells are given; edges between two of its own cells are not drawn */
static void block_outline(const int cells[8], int th) {
  int k, j;
  for (k = 0; k < 8; k++) {
    int r = cells[k] / PG_SW_COLS, c = cells[k] % PG_SW_COLS, x = sbx + c * scs, y = sby + r * scs, nb[4] = {0, 0, 0, 0};
    for (j = 0; j < 8; j++) {
      if (cells[j] == cells[k] - PG_SW_COLS) nb[0] = 1;
      if (cells[j] == cells[k] + PG_SW_COLS) nb[1] = 1;
      if (c > 0 && cells[j] == cells[k] - 1) nb[2] = 1;
      if (c < PG_SW_COLS - 1 && cells[j] == cells[k] + 1) nb[3] = 1;
    }
    if (!nb[0]) FillArea(x, y, scs + 1, th, BLACK);
    if (!nb[1]) FillArea(x, y + scs - th + 1, scs + 1, th, BLACK);
    if (!nb[2]) FillArea(x, y, th, scs + 1, BLACK);
    if (!nb[3]) FillArea(x + scs - th + 1, y, th, scs + 1, BLACK);
  }
}

static void draw_swap(const PgState *s) {
  int i, b, k, cells[8];
  FillArea(sbx, sby, scs * PG_SW_COLS, scs * PG_SW_ROWS, WHITE);
  for (i = 1; i < PG_SW_COLS; i++) DrawLine(sbx + i * scs, sby, sbx + i * scs, sby + scs * PG_SW_ROWS, 0xDDDDDD);
  for (i = 1; i < PG_SW_ROWS; i++) DrawLine(sbx, sby + i * scs, sbx + scs * PG_SW_COLS, sby + i * scs, 0xDDDDDD);
  frame(sbx - 3, sby - 3, scs * PG_SW_COLS + 6, scs * PG_SW_ROWS + 6, 3, BLACK);
  for (b = 0; b < PG_BLOCKS; b++) {
    pg_swap_cells(b, s, cells);
    for (k = 0; k < 8; k++) FillArea(sbx + (cells[k] % PG_SW_COLS) * scs, sby + (cells[k] / PG_SW_COLS) * scs, scs, scs, block_fill(b));
    block_outline(cells, b == sel_block ? 5 : 2);
  }
  if (sel_block >= 0) {
    if (prev_org >= 0) { /* the ghost of the landing place */
      PgState g = *s;
      g.org[sel_block] = (unsigned char)prev_org;
      pg_swap_cells(sel_block, &g, cells);
      for (k = 0; k < 8; k++) {
        int x = sbx + (cells[k] % PG_SW_COLS) * scs, y = sby + (cells[k] / PG_SW_COLS) * scs;
        FillArea(x + 2, y + 2, scs - 3, scs - 3, 0xBBBBBB);
      }
      block_outline(cells, 4);
    }
    for (i = 0; i < PG_SW_CELLS; i++) {
      PgState g = *s;
      int a, cx, cy;
      if (!block_dest[i]) continue;
      g.org[sel_block] = (unsigned char)i;
      a = pg_swap_anchor(sel_block, &g);
      cx = sbx + (a % PG_SW_COLS) * scs + scs / 2;
      cy = sby + (a / PG_SW_COLS) * scs + scs / 2;
      if (i == prev_org) ring_circle(cx, cy, scs * 4 / 10, 3, WHITE);
      fill_circle(cx, cy, i == prev_org ? scs / 6 : scs / 5, i == prev_org ? BLACK : 0x111111);
    }
  }
}

/* ---------- screens ---------- */

static void draw_status(void) {
  char t[400], one[120];
  const char *hint = "";
  int solved_now = pg_solved(cur, &st[cur]), len;
  if (reset_armed) hint = tr(&S_RESET_AGAIN);
  else if (msg[0]) hint = msg;
  len = snprintf(t, sizeof t, tr(&S_MOVES), nmoves[cur]);
  if (best[cur] > 0) {
    snprintf(one, sizeof one, tr(&S_BEST), best[cur]);
    len += snprintf(t + len, sizeof t - (size_t)len, "   %s", one);
  }
  if (pg_par(cur)) {
    snprintf(one, sizeof one, tr(&S_MIN), pg_par(cur));
    len += snprintf(t + len, sizeof t - (size_t)len, "   %s", one);
  }
  if (is_tiles() && !solved_now) {
    snprintf(one, sizeof one, tr(&S_PLACED), pg_placed(cur, &st[cur]), pg_nslots(cur));
    len += snprintf(t + len, sizeof t - (size_t)len, "\n%s", one);
  }
  if (!hint[0]) {
    if (solved_now) {
      if (pg_par(cur)) snprintf(one, sizeof one, tr(&S_SOLVED_N), nmoves[cur]);
      else snprintf(one, sizeof one, "%s", tr(&S_SOLVED));
      len += snprintf(t + len, sizeof t - (size_t)len, "\n%s", one);
      if (pg_par(cur) && nmoves[cur] <= pg_par(cur)) len += snprintf(t + len, sizeof t - (size_t)len, " %s", tr(&S_OPTIMAL));
      snprintf(t + len, sizeof t - (size_t)len, " %s", tr(&S_NEXT));
      hint = NULL;
    } else if (is_tiles() && pg_placed(cur, &st[cur]) == pg_nslots(cur)) {
      int c = pg_tile_check(cur, &st[cur]);
      hint = c == 2 ? tr(&S_LOOSE) : c == 3 ? tr(&S_LOOPS) : tr(&S_LINES);
    } else if (is_graph()) hint = sel_node >= 0 ? tr(&S_HINT_DEST) : tr(&S_HINT_GRAPH);
    else if (is_tiles()) hint = sel_slot >= 0 ? tr(&S_HINT_LAID) : sel_tray >= 0 ? tr(&S_HINT_PLACE) : tr(&S_HINT_TRAY);
    else hint = sel_block < 0 ? tr(&S_HINT_BLOCK) : prev_org >= 0 ? tr(&S_HINT_MOVE) : tr(&S_HINT_DOT);
  }
  if (hint && hint[0]) snprintf(t + len, sizeof t - (size_t)len, "\n%s", hint);
  SetFont(ui_f_small, BLACK);
  DrawTextRect(st_x, st_y, st_w, st_h, t, ALIGN_CENTER | VALIGN_MIDDLE);
}

#ifdef PG_GEOM_LOG
/* test builds only: the screen positions of what can be tapped, for tools/test-rebus-ui.sh */
static void log_geometry(int W, int H) {
  int i, x, y;
  ui_log("pgg %d %d %d", cur, W, H);
  if (is_graph()) for (i = 0; i < pg_nnodes(cur); i++) ui_log("pgn %d %d %d", i, nx[i], ny[i]);
  else if (is_tiles()) {
    for (i = 0; i < pg_nslots(cur); i++) { slot_origin(i, &x, &y); ui_log("pgs %d %d %d", i, x + tile_cells * tcs / 2, y + tile_cells * tcs / 2); }
    for (i = 0; i < pg_ntiles(cur); i++) { tray_origin(i, &x, &y); ui_log("pgt %d %d %d", i, x + tray_tile / 2, y + tray_tile / 2); }
  } else ui_log("pgc %d %d %d", sbx, sby, scs);
}
#endif

static void draw_play(void) {
  char t[120], name[80];
  int i, solved_flag = solved[cur];
  snprintf(name, sizeof name, "%s", tr(&S_GAME[cur]));
  snprintf(t, sizeof t, tr(&S_TITLE), name, cur + 1, PG_GAMES, solved_flag ? "  ok" : "");
  { /* the longer names (German especially) need a smaller font to stay between the buttons */
    ifont *f = ui_f_title;
    SetFont(f, BLACK);
    if (StringWidth(t) > title_w) f = ui_f_small;
    SetFont(f, BLACK);
    if (StringWidth(t) > title_w) f = ui_f_tiny;
    ui_text_center(f, BLACK, title_x, 8, title_w, btns[0].h, t);
  }
  for (i = 0; i < nbtn; i++) ui_draw_btn(&btns[i]);
  if (is_graph()) draw_graph(&st[cur]);
  else if (is_tiles()) draw_tiles(&st[cur]);
  else draw_swap(&st[cur]);
  if (gp_on) draw_goal();
  draw_status();
}

static void draw_list(int W, int H) {
  int i, m = 12, top = 70, gap = 8, bwid = 200, rows = PG_GAMES;
  int rh = (H - top - 90 - (rows - 1) * gap) / rows;
  if (rh > 90) rh = 90;
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_LIST_TITLE));
  nbtn = 0;
  for (i = 0; i < PG_GAMES; i++) {
    int y = top + i * (rh + gap), x = m, w = W - 2 * m;
    char t[64], status[64];
    ui_box(x, y, w, rh, i == cur ? 4 : 2, solved[i] ? 0xDDDDDD : WHITE);
    snprintf(t, sizeof t, "%d. %s", i + 1, tr(&S_GAME[i]));
    if (solved[i] && best[i] > 0) snprintf(status, sizeof status, tr(&S_LS_BEST), best[i]);
    else snprintf(status, sizeof status, "%s", tr(solved[i] ? &S_LS_DONE : nmoves[i] > 0 ? &S_LS_PLAY : &S_LS_NEW));
    SetFont(ui_f_btn, BLACK);
    DrawString(x + 14, y + (rh - ui_f_btn->height) / 2, t);
    SetFont(ui_f_small, BLACK);
    DrawString(x + w - 14 - StringWidth(status), y + (rh - ui_f_small->height) / 2, status);
    add_btn(x, y, w, rh, 100 + i, "");
  }
  {
    int bx = (W - 2 * bwid - 20) / 2, by = H - 80;
    add_btn(bx, by, bwid, 64, B_CLOSE, tr(&S_BACK));
    add_btn(bx + bwid + 20, by, bwid, 64, B_HOME, tr(&S_HOME));
    ui_draw_btn(&btns[nbtn - 2]);
    ui_draw_btn(&btns[nbtn - 1]);
  }
}

static void draw_help(int W, int H) {
  char t[1600];
  ui_text_center(ui_f_title, BLACK, 0, 8, W, 50, tr(&S_GAME[cur]));
  SetFont(ui_f_small, BLACK);
  snprintf(t, sizeof t, "%s\n\n%s", tr(&S_RULES[cur]), tr(&S_TAP_CLOSE));
  DrawTextRect(16, 64, W - 32, H - 80, t, ALIGN_LEFT);
}

static void redraw(int full) {
  int W = ScreenWidth(), H = ScreenHeight();
  ui_fonts(28);
  if (mode == M_PLAY) layout(W, H);
  ClearScreen();
  if (mode == M_PLAY) {
    draw_play();
#ifdef PG_GEOM_LOG
    log_geometry(W, H);
#endif
  } else if (mode == M_LIST) draw_list(W, H);
  else draw_help(W, H);
  updates++;
  if (full || updates % 8 == 0) FullUpdate();
  else PartialUpdate(0, 0, W, H);
}

/* ---------- actions ---------- */

static void clear_selection(void) {
  sel_node = -1;
  sel_tray = sel_slot = sel_block = prev_org = -1;
  sel_rot = 0;
}

static void open_game(int i) {
  cur = (i % PG_GAMES + PG_GAMES) % PG_GAMES;
  reset_armed = 0;
  msg[0] = 0;
  clear_selection();
  save_state();
}

/* records a move that pg_apply already accepted... or applies and records it; returns 1 if it was applied */
static int do_move(PgMove m) {
  if (nmoves[cur] >= MAXMOVES) { strcpy(msg, tr(&S_FULL)); return 0; }
  if (!pg_apply(cur, &st[cur], m)) return 0;
  stack[cur][nmoves[cur]++] = m;
  stamp[cur] = ui_now_ms();
  if (pg_solved(cur, &st[cur])) {
    solved[cur] = 1;
    if (best[cur] == 0 || nmoves[cur] < best[cur]) best[cur] = nmoves[cur];
  }
  save_state();
  return 1;
}

static void undo(void) {
  reset_armed = 0;
  msg[0] = 0;
  clear_selection();
  if (nmoves[cur] == 0) return;
  nmoves[cur]--;
  pg_replay(cur, &st[cur], stack[cur], nmoves[cur]);
  stamp[cur] = ui_now_ms();
  save_state();
}

static void reset_game(void) {
  nmoves[cur] = 0;
  pg_init(cur, &st[cur]);
  stamp[cur] = ui_now_ms();
  reset_armed = 0;
  msg[0] = 0;
  clear_selection();
  save_state();
}

/* a tap on the board; returns 1 if a game was just solved */
static int tap_graph(int x, int y) {
  int i, hit = -1, bestd = (nr + 8) * (nr + 8), was = solved[cur];
  msg[0] = 0;
  reset_armed = 0;
  for (i = 0; i < pg_nnodes(cur); i++) {
    int d = (nx[i] - x) * (nx[i] - x) + (ny[i] - y) * (ny[i] - y);
    if (d < bestd) { bestd = d; hit = i; }
  }
  if (hit < 0) return 0;
  if (sel_node >= 0 && node_dest[hit]) {
    PgMove m = {PGM_SLIDE, (unsigned char)sel_node, (unsigned char)hit, 0};
    clear_selection();
    do_move(m);
    return !was && solved[cur];
  }
  if (st[cur].tok[hit]) {
    if (hit == sel_node) { clear_selection(); return 0; }
    clear_selection();
    sel_node = hit;
    if (!pg_graph_dests(cur, &st[cur], hit, node_dest)) { strcpy(msg, tr(&S_NO_MOVES)); sel_node = -1; }
  }
  return 0;
}

static int tap_tiles(int x, int y) {
  int ncell = board_side * tile_cells, t, was = solved[cur];
  msg[0] = 0;
  reset_armed = 0;
  if (x >= tbx && x < tbx + ncell * tcs && y >= tby && y < tby + ncell * tcs) {
    int slot = ((y - tby) / (tile_cells * tcs)) * board_side + (x - tbx) / (tile_cells * tcs);
    if (st[cur].tile[slot] < 0) {
      if (sel_tray >= 0) {
        PgMove m = {PGM_PLACE, (unsigned char)sel_tray, (unsigned char)slot, (unsigned char)sel_rot};
        clear_selection();
        do_move(m);
        return !was && solved[cur];
      }
    } else if (sel_slot == slot) { /* a second tap turns the laid tile */
      PgMove m = {PGM_TURN, (unsigned char)slot, (unsigned char)((st[cur].rot[slot] + 1) & 3), 0};
      do_move(m);
      return !was && solved[cur];
    } else {
      clear_selection();
      sel_slot = slot;
    }
    return 0;
  }
  for (t = 0; t < pg_ntiles(cur); t++) {
    int tx, ty, sl;
    tray_origin(t, &tx, &ty);
    if (x < tx || x >= tx + tray_tile || y < ty || y >= ty + tray_tile) continue;
    sl = pg_tile_slot(cur, &st[cur], t);
    if (sl >= 0) { clear_selection(); sel_slot = sl; }
    else if (sel_tray == t) sel_rot = (sel_rot + 1) & 3;
    else { clear_selection(); sel_tray = t; }
    return 0;
  }
  return 0;
}

static int tap_swap(int x, int y) {
  int cell, was = solved[cur];
  msg[0] = 0;
  reset_armed = 0;
  if (x < sbx || x >= sbx + scs * PG_SW_COLS || y < sby || y >= sby + scs * PG_SW_ROWS) return 0;
  cell = ((y - sby) / scs) * PG_SW_COLS + (x - sbx) / scs;
  if (sel_block >= 0) {
    int o = pg_swap_origin_for_anchor(sel_block, cell);
    if (o >= 0 && block_dest[o]) {
      if (o == prev_org) {
        PgMove m = {PGM_BLOCK, (unsigned char)sel_block, (unsigned char)o, 0};
        clear_selection();
        do_move(m);
        return !was && solved[cur];
      }
      prev_org = o;
      return 0;
    }
  }
  {
    int b = pg_swap_block_at(&st[cur], cell);
    if (b >= 0 && b != sel_block) {
      clear_selection();
      sel_block = b;
      if (!pg_swap_dests(&st[cur], b, block_dest)) { strcpy(msg, tr(&S_NO_MOVES_B)); sel_block = -1; }
    } else clear_selection();
  }
  return 0;
}

static void do_button(int id) {
  if (id >= 100 && id < 100 + PG_GAMES) { mode = M_PLAY; open_game(id - 100); return; }
  switch (id) {
    case B_UNDO: undo(); break;
    case B_RESET:
      if (!reset_armed) reset_armed = 1;
      else reset_game();
      break;
    case B_PREV: open_game(cur - 1); break;
    case B_NEXT: open_game(cur + 1); break;
    case B_LIST: mode = M_LIST; break;
    case B_HELP: mode = M_HELP; break;
    case B_CLOSE: mode = M_PLAY; break;
    case B_ROT:
      reset_armed = 0;
      msg[0] = 0;
      if (sel_tray >= 0) sel_rot = (sel_rot + 1) & 3;
      else if (sel_slot >= 0) { PgMove m = {PGM_TURN, (unsigned char)sel_slot, (unsigned char)((st[cur].rot[sel_slot] + 1) & 3), 0}; do_move(m); }
      else strcpy(msg, tr(&S_PICK_FIRST));
      break;
    case B_TRAY:
      reset_armed = 0;
      msg[0] = 0;
      if (sel_slot >= 0) { PgMove m = {PGM_TAKE, (unsigned char)sel_slot, 0, 0}; clear_selection(); do_move(m); }
      else strcpy(msg, tr(&S_PICK_FIRST));
      break;
    case B_HOME: left = 1; shell_leave(); return;
    default: break;
  }
}

static void tap(int x, int y) {
  int i;
  if (mode == M_HELP) { mode = M_PLAY; redraw(1); return; }
  for (i = 0; i < nbtn; i++) {
    const Btn *b = &btns[i];
    if (ui_hit(b, x, y)) {
      int before = mode, id = b->id;
      do_button(id);
      if (left) { left = 0; return; }
      redraw(mode != before || id == B_PREV || id == B_NEXT || id == B_RESET || id == B_UNDO || id >= 100);
      return;
    }
  }
  if (mode != M_PLAY) return;
  {
    int won = is_graph() ? tap_graph(x, y) : is_tiles() ? tap_tiles(x, y) : tap_swap(x, y);
    redraw(won);
  }
}

/* ---------- Module interface (shell.h) ---------- */

static void playground_init(void) {
  int g;
  for (g = 0; g < PG_GAMES; g++) pg_init(g, &st[g]);
  cur = 0;
  load_state();
}

static void playground_enter(int puzzle) {
  mode = M_PLAY;
  if (puzzle >= 0) open_game(puzzle);
  else { reset_armed = 0; msg[0] = 0; clear_selection(); }
  redraw(1);
}

static int playground_event(int type, int par1, int par2) {
  switch (type) {
    case EVT_SHOW:
    case EVT_ORIENTATION:
      redraw(1);
      break;
    case EVT_POINTERUP:
      tap(par1, par2);
      break;
    case EVT_KEYPRESS:
      if (par1 == KEY_BACK) {
        if (mode != M_PLAY) { mode = M_PLAY; redraw(1); }
        else shell_leave();
      } else if (par1 == KEY_HOME || par1 == KEY_MENU) {
        shell_leave(); /* the reader's hardware button: straight back to the home screen */
      } else if (par1 == KEY_PREV && mode == M_PLAY) {
        open_game(cur - 1); redraw(1);
      } else if (par1 == KEY_NEXT && mode == M_PLAY) {
        open_game(cur + 1); redraw(1);
      }
      break;
    default:
      break;
  }
  return 0;
}

/* ---------- progress file hooks (shell.h) ---------- */

static int playground_prog_count(void) { return PG_GAMES; }
static const char *playground_prog_id(int i) { return pg_id(i); }

/* the game's start position in the source's own JSON fields (what its stateToJson writes): only solved games are exported, as
 * "solved, best N moves" on a start-position board with an empty move list, which is a valid entry for the app */
static const char *start_json(int g) {
  static char b[1200];
  PgState s;
  int i, n = 0, N = (int)sizeof b;
  pg_init(g, &s);
  if (pg_kind(g) == PGK_GRAPH) {
    n += snprintf(b + n, (size_t)(N - n), "\"positions\": {");
    for (i = 0; i < pg_nnodes(g); i++) {
      char tok[8];
      if (!s.tok[i]) strcpy(tok, "null");
      else if (g == PG_EIGHT) snprintf(tok, sizeof tok, "\"%d\"", s.tok[i]);
      else if (g == PG_CATS) snprintf(tok, sizeof tok, "\"%c\"", s.tok[i] == 1 ? 'C' : 'D');
      else strcpy(tok, "\"T\"");
      n += snprintf(b + n, (size_t)(N - n), "%s\"%s\": %s", i ? ", " : "", pg_node_name(g, i), tok);
    }
    snprintf(b + n, (size_t)(N - n), "}");
  } else if (pg_kind(g) == PGK_TILES) {
    n += snprintf(b + n, (size_t)(N - n), "\"slots\": [");
    for (i = 0; i < pg_nslots(g); i++) n += snprintf(b + n, (size_t)(N - n), "%snull", i ? ", " : "");
    snprintf(b + n, (size_t)(N - n), "]");
  } else {
    n += snprintf(b + n, (size_t)(N - n), "\"origins\": [");
    for (i = 0; i < PG_BLOCKS; i++) n += snprintf(b + n, (size_t)(N - n), "%s%d", i ? ", " : "", s.org[i]);
    snprintf(b + n, (size_t)(N - n), "]");
  }
  return b;
}

static int playground_prog_get(int i, ProgEntry *e) {
  if (!solved[i]) return 0; /* a half-played game is not exported: its moves cannot travel, and an entry without them would wipe the app's */
  e->solved = 1;
  e->updated_at = stamp[i];
  e->best_moves = best[i];
  e->state_json = start_json(i);
  return 1;
}

/* an incoming solved game: "solved" and the better move count are taken over, the board stays as it is; a best that is
 * below the proven minimum is impossible and ignored */
static void playground_prog_put(int i, const ProgEntry *e) {
  if (!e->solved) return;
  solved[i] = 1;
  if (e->best_moves > 0 && e->best_moves >= pg_par(i) && (best[i] == 0 || e->best_moves < best[i])) best[i] = e->best_moves;
  if (e->updated_at > 0) stamp[i] = e->updated_at;
}

static int playground_count(void) { return PG_GAMES; }

static int playground_solved(void) {
  int g, n = 0;
  for (g = 0; g < PG_GAMES; g++) n += solved[g];
  return n;
}

static int playground_pick_random(void) {
  int g, n = 0, want;
  for (g = 0; g < PG_GAMES; g++) n += !solved[g];
  if (n == 0) return rand() % PG_GAMES;
  want = rand() % n;
  for (g = 0; g < PG_GAMES; g++)
    if (!solved[g] && want-- == 0) return g;
  return 0;
}

const Module module_playground = {
    "playground",
    S("Puzzle games", "Головоломки-игры", "Denkspiele"),
    S("Seven small play-puzzles: chips, tiles, blocks.", "Семь маленьких головоломок-игр: фишки, плитки, блоки.",
      "Sieben kleine Spielrätsel: Chips, Plättchen, Blöcke."),
    playground_init,
    playground_enter,
    playground_event,
    save_state,
    playground_count,
    playground_solved,
    playground_pick_random,
    "module_playground",
    playground_prog_count,
    playground_prog_id,
    playground_prog_get,
    playground_prog_put,
};
