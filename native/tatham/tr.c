#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tr.h"

#ifndef TAT_DIR
#define TAT_DIR "/mnt/ext1/tatham"
#endif
/* The firmware keeps its own language here as a line `language=ru`. */
#ifndef SYSCFGF
#define SYSCFGF "/mnt/ext1/system/config/global.cfg"
#endif

/* ---- config file (key=value lines) ---- */

const char *cfg_path(void) { return TAT_DIR "/tatham.cfg"; }

int cfg_get(const char *key, char *out, size_t n)
{
    char line[256];
    size_t kl = strlen(key);
    FILE *f = fopen(cfg_path(), "r");
    int found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            size_t l;
            snprintf(out, n, "%s", line + kl + 1);
            l = strlen(out);
            while (l && (out[l - 1] == '\n' || out[l - 1] == '\r')) out[--l] = 0;
            found = 1;
        }
    }
    fclose(f);
    return found;
}

void cfg_set(const char *key, const char *value)
{
    char lines[16][256];
    int n = 0, i, done = 0;
    size_t kl = strlen(key);
    FILE *f = fopen(cfg_path(), "r");
    if (f) {
        while (n < 16 && fgets(lines[n], sizeof lines[n], f)) {
            if (strncmp(lines[n], key, kl) == 0 && lines[n][kl] == '=') {
                snprintf(lines[n], sizeof lines[n], "%s=%s\n", key, value);
                done = 1;
            }
            n++;
        }
        fclose(f);
    }
    if (!done && n < 16) snprintf(lines[n++], sizeof lines[0], "%s=%s\n", key, value);
    f = fopen(cfg_path(), "w");
    if (!f) return;
    for (i = 0; i < n; i++) fputs(lines[i], f);
    fclose(f);
}

/* ---- language selection ---- */

static int setting = LANG_SYSTEM, system_lang = LANG_EN;

static int code_to_lang(const char *c)
{
    if (strncmp(c, "ru", 2) == 0) return LANG_RU;
    if (strncmp(c, "de", 2) == 0) return LANG_DE;
    return LANG_EN;
}

void tr_load(void)
{
    char line[128], v[64];
    FILE *f = fopen(SYSCFGF, "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (strncmp(line, "language=", 9) == 0) system_lang = code_to_lang(line + 9);
        fclose(f);
    }
    if (cfg_get("lang", v, sizeof v) && strncmp(v, "system", 6) != 0) setting = code_to_lang(v);
    else setting = LANG_SYSTEM;
}

int tr_setting(void) { return setting; }
int tr_lang(void) { return setting == LANG_SYSTEM ? system_lang : setting; }

void tr_set_setting(int s)
{
    static const char *const CODE[NUM_LANGS] = {"en", "ru", "de"};
    setting = (s >= 0 && s < NUM_LANGS) ? s : LANG_SYSTEM;
    cfg_set("lang", setting == LANG_SYSTEM ? "system" : CODE[setting]);
}

/* ---- UI strings: en, ru, de ---- */

static const char *const UI[S_COUNT][NUM_LANGS] = {
    [S_TITLE] = {"Puzzle Collection", "Коллекция головоломок", "Rätselsammlung"},
    [S_CREDIT] = {"Simon Tatham's Portable Puzzle Collection (MIT licence)",
                  "Simon Tatham's Portable Puzzle Collection (лицензия MIT)",
                  "Simon Tatham's Portable Puzzle Collection (MIT-Lizenz)"},
    [S_GAMES] = {"Games", "Игры", "Spiele"},
    [S_NEW] = {"New", "Новая", "Neu"},
    [S_UNDO] = {"Undo", "Отмена", "Zurück"},
    [S_REDO] = {"Redo", "Повтор", "Wieder"},
    [S_MENU] = {"Menu", "Меню", "Menü"},
    [S_BTN_LEFT] = {"Left button", "Левая кнопка", "Linke Taste"},
    [S_BTN_RIGHT] = {"Right button", "Правая кнопка", "Rechte Taste"},
    [S_RESTART] = {"Restart this puzzle", "Начать заново", "Neu starten"},
    [S_SOLVE] = {"Show solution", "Показать решение", "Lösung zeigen"},
    [S_TYPE] = {"Size and difficulty", "Размер и сложность", "Größe und Schwierigkeit"},
    [S_CUSTOM] = {"Custom...", "Свои параметры...", "Eigene Werte..."},
    [S_RULES] = {"Rules and controls", "Правила и управление", "Regeln und Bedienung"},
    [S_LANGUAGE] = {"Language", "Язык", "Sprache"},
    [S_CLEAN] = {"Clean the screen", "Очистить экран", "Bildschirm bereinigen"},
    [S_THICK_ON] = {"Thick lines: on", "Толстые линии: вкл.", "Dicke Linien: an"},
    [S_THICK_OFF] = {"Thick lines: off", "Толстые линии: выкл.", "Dicke Linien: aus"},
    [S_EXIT] = {"Exit", "Выход", "Beenden"},
    [S_BACK] = {"Back", "Назад", "Zurück"},
    [S_OK] = {"OK", "OK", "OK"},
    [S_CANCEL] = {"Cancel", "Отмена", "Abbrechen"},
    [S_PAGE] = {"Page", "Стр.", "Seite"},
    [S_GENERATING] = {"Generating the puzzle...", "Создаю головоломку...", "Rätsel wird erzeugt..."},
    [S_SOLVED] = {"Solved!", "Решено!", "Gelöst!"},
    [S_SYSTEM] = {"System default", "Как в системе", "Systemstandard"},
    [S_NEW_Q] = {"Start a new puzzle? The current one will be lost.",
                 "Начать новую головоломку? Текущая будет потеряна.",
                 "Neues Rätsel beginnen? Das aktuelle geht verloren."},
    [S_YES] = {"Yes", "Да", "Ja"},
    [S_NO] = {"No", "Нет", "Nein"},
    [S_TIP] = {"Tip: hold a finger on the board for the right button, or switch it with the button at the bottom. "
               "Hardware keys: page keys = undo and redo, back = games list.",
               "Подсказка: долгое нажатие на поле = правая кнопка, или переключите её кнопкой внизу. "
               "Клавиши: листание = отмена и повтор, назад = список игр.",
               "Tipp: Finger auf dem Feld halten = rechte Taste, oder unten umschalten. "
               "Tasten: Blättern = Zurück und Wieder, Zurück = Spieleliste."},
    [S_PREV] = {"< Prev", "< Назад", "< Zurück"},
    [S_NEXT] = {"Next >", "Далее >", "Weiter >"},
    [S_SAVED] = {"saved", "сохранена", "gespeichert"},
    [S_ERROR] = {"Error", "Ошибка", "Fehler"},
    [S_CUSTOM_TITLE] = {"Custom settings", "Свои параметры", "Eigene Werte"},
    [S_HOME_SAVED] = {"in progress", "начата", "begonnen"},
    [S_HOME_DONE] = {"solved", "решена", "gelöst"},
    [S_SOLVE_NO] = {"This puzzle has no solution display.", "Для этой игры нет показа решения.",
                    "Für dieses Rätsel gibt es keine Lösungsanzeige."},
    [S_EXPORT] = {"Export all saves (zip)", "Экспорт всех сохранений (zip)", "Alle Spielstände exportieren (zip)"},
    [S_IMPORT] = {"Import saves (zip)", "Импорт сохранений (zip)", "Spielstände importieren (zip)"},
    [S_EXPORTED] = {"Saved %d puzzles to %s", "Сохранено головоломок: %d в %s", "%d Rätsel gespeichert in %s"},
    [S_IMPORTED] = {"Restored %d puzzles", "Восстановлено головоломок: %d", "%d Rätsel wiederhergestellt"},
    [S_IMPORT_NONE] = {"No readable zip found: %s", "Не найден читаемый zip: %s", "Keine lesbare Zip-Datei: %s"},
    [S_IMPORT_COMPRESSED] = {"This zip is compressed. Export it again from the app (uncompressed zip), or re-pack it with \"store\".",
                             "Этот zip сжат. Экспортируйте его заново из приложения (zip без сжатия) или перепакуйте с режимом «без сжатия».",
                             "Diese Zip-Datei ist komprimiert. Exportiere sie erneut aus der App (unkomprimiert) oder packe sie ohne Kompression neu."},
    [S_EXPORT_FAIL] = {"Could not write %s", "Не удалось записать %s", "%s konnte nicht geschrieben werden"},
};

const char *T(int id)
{
    const char *s;
    if (id < 0 || id >= S_COUNT) return "";
    s = UI[id][tr_lang()];
    return s ? s : UI[id][LANG_EN];
}

/* ---- puzzle names (en as in the engine) ---- */

static const char *const NAMES[][NUM_LANGS] = {
    {"Black Box", "Чёрный ящик", "Black Box"},
    {"Bridges", "Мосты", "Brücken"},
    {"Cube", "Куб", "Würfel"},
    {"Dominosa", "Доминоза", "Dominosa"},
    {"Fifteen", "Пятнашки", "Fünfzehn"},
    {"Filling", "Заполнение", "Füllen"},
    {"Flip", "Переворот", "Umschalten"},
    {"Flood", "Заливка", "Flut"},
    {"Galaxies", "Галактики", "Galaxien"},
    {"Guess", "Код-замок", "Code knacken"},
    {"Inertia", "Инерция", "Trägheit"},
    {"Keen", "Кен-Кен", "Keen"},
    {"Light Up", "Лампочки", "Lichter an"},
    {"Loopy", "Слитерлинк", "Loopy"},
    {"Magnets", "Магниты", "Magnete"},
    {"Map", "Раскраска карты", "Landkarte"},
    {"Mines", "Сапёр", "Minensuche"},
    {"Mosaic", "Мозаика", "Mosaik"},
    {"Net", "Сеть", "Netz"},
    {"Netslide", "Сеть со сдвигом", "Netz-Schieber"},
    {"Palisade", "Частокол", "Palisade"},
    {"Pattern", "Нонограмма", "Nonogramm"},
    {"Pearl", "Жемчуг", "Perlen"},
    {"Pegs", "Солитёр", "Solitär"},
    {"Range", "Куромасу", "Kuromasu"},
    {"Rectangles", "Прямоугольники", "Rechtecke"},
    {"Same Game", "Одинаковые", "Same Game"},
    {"Signpost", "Указатели", "Wegweiser"},
    {"Singles", "Одиночки", "Singles"},
    {"Sixteen", "Шестнадцать", "Sechzehn"},
    {"Slant", "Диагонали", "Schräg"},
    {"Solo", "Судоку", "Sudoku"},
    {"Tents", "Палатки", "Zelte"},
    {"Towers", "Башни", "Türme"},
    {"Train Tracks", "Железная дорога", "Gleise"},
    {"Twiddle", "Вращалки", "Drehen"},
    {"Undead", "Нежить", "Untote"},
    {"Unequal", "Неравенства", "Ungleich"},
    {"Unruly", "Двоичный пазл", "Binärrätsel"},
    {"Untangle", "Распутывание", "Entwirren"},
};

const char *tr_game_name(const char *en)
{
    size_t i;
    int l = tr_lang();
    if (l == LANG_EN) return en;
    for (i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++)
        if (strcmp(NAMES[i][LANG_EN], en) == 0) return NAMES[i][l];
    return en;
}

/* ---- glossary: words and phrases in preset names, option labels and status texts ---- */

typedef struct { const char *en, *ru, *de; } Gloss;

static const Gloss GLOSS[] = {
    /* difficulty */
    {"Unreasonable", "Безумно", "Unvernünftig"}, {"unreasonable", "безумно", "unvernünftig"},
    {"Extreme", "Экстрим", "Extrem"}, {"extreme", "экстрим", "extrem"},
    {"Intermediate", "Средне", "Mittel"}, {"Advanced", "Продвинуто", "Fortgeschritten"},
    {"Recursive", "Рекурсивно", "Rekursiv"}, {"Ambiguous", "Неоднозначно", "Mehrdeutig"},
    {"Trivial", "Тривиально", "Trivial"}, {"Basic", "Базово", "Einfach"},
    {"Medium", "Средне", "Mittel"}, {"medium", "средне", "mittel"},
    {"Normal", "Нормально", "Normal"}, {"normal", "нормально", "normal"},
    {"Tricky", "Хитро", "Knifflig"}, {"tricky", "хитро", "knifflig"},
    {"Easy", "Легко", "Leicht"}, {"easy", "легко", "leicht"},
    {"Hard", "Сложно", "Schwer"}, {"hard", "сложно", "schwer"},
    {"Super", "Супер", "Super"}, {"Standard", "Стандарт", "Standard"},
    {"Small", "Малый", "Klein"}, {"small", "малый", "klein"}, {"Large", "Большой", "Groß"},
    {"Random", "Случайно", "Zufällig"}, {"More", "Больше", "Mehr"},
    /* sizes and kinds in preset names */
    {"Order", "Порядок", "Ordnung"}, {"Size", "Размер", "Größe"}, {"size", "размер", "Größe"},
    {"mines", "мин", "Minen"}, {"regions", "областей", "Regionen"}, {"colours", "цветов", "Farben"},
    {"points", "точек", "Punkte"}, {"balls", "шаров", "Kugeln"}, {"clues", "подсказок", "Hinweise"},
    {"blocks", "блоков", "Blöcke"}, {"rows", "рядов", "Reihen"}, {"strip", "полоса", "Streifen"},
    {"rotating", "вращение", "Drehen"}, {"only", "только", "nur"}, {"free", "свободно", "frei"},
    {"ends", "концы", "Enden"}, {"orientable", "ориентируемый", "orientierbar"},
    {"multiplication", "умножение", "Multiplikation"}, {"Jigsaw", "Пазл", "Puzzle"},
    {"Killer", "Киллер", "Killer"}, {"Adjacent", "Соседние", "Benachbart"}, {"Unequal", "Неравенства", "Ungleich"},
    {"wrapping", "замкнутая", "umlaufend"}, {"Squares", "Квадраты", "Quadrate"}, {"Square", "Квадрат", "Quadrat"},
    {"Triangular", "Треугольная", "Dreieckig"}, {"Honeycomb", "Соты", "Waben"}, {"Octagonal", "Восьмиугольная", "Achteckig"},
    {"Great-Great-Dodecagonal", "Большая-большая двенадцатиугольная", "Groß-groß-zwölfeckig"},
    {"Great-Dodecagonal", "Большая двенадцатиугольная", "Groß-zwölfeckig"},
    {"Great Hexagonal", "Большая шестиугольная", "Großsechseckig"}, {"Great-Hexagonal", "Большая шестиугольная", "Großsechseckig"},
    {"Compass-Dodecagonal", "Компасная двенадцатиугольная", "Kompass-zwölfeckig"},
    {"Dodecagonal", "Двенадцатиугольная", "Zwölfeckig"}, {"Snub-Square", "Курносая квадратная", "Quadrat-Dreieck"},
    {"Penrose (kite/dart)", "Пенроуз (змей/дротик)", "Penrose (Drachen/Pfeil)"},
    {"Penrose (rhombs)", "Пенроуз (ромбы)", "Penrose (Rauten)"},
    {"Penrose Kite/Dart", "Пенроуз (змей/дротик)", "Penrose (Drachen/Pfeil)"},
    {"Penrose Rhombs", "Пенроуз (ромбы)", "Penrose (Rauten)"}, {"Penrose", "Пенроуз", "Penrose"},
    {"Kites", "Воздушные змеи", "Drachen"}, {"Kagome", "Кагомэ", "Kagome"}, {"Floret", "Цветок", "Blüte"},
    {"Cairo", "Каирская", "Kairo"}, {"Hats", "Шляпы", "Hüte"}, {"Spectres", "Призраки", "Spektren"},
    {"Tetrahedron", "Тетраэдр", "Tetraeder"}, {"Octahedron", "Октаэдр", "Oktaeder"},
    {"Icosahedron", "Икосаэдр", "Ikosaeder"}, {"Octagon", "Восьмиугольник", "Achteck"},
    {"Crosses", "Кресты", "Kreuze"}, {"Cross", "Крест", "Kreuz"},
    /* option labels (config dialogs) */
    {"Number of shuffling moves", "Число перемешиваний", "Anzahl Mischzüge"},
    {"Walls wrap around", "Края замкнуты", "Ränder umlaufend"},
    {"Ensure unique solution", "Единственное решение", "Eindeutige Lösung"},
    {"Ensure solubility", "Гарантировать решаемость", "Lösbarkeit sichern"},
    {"Allow unsoluble", "Допустить нерешаемые", "Unlösbar erlauben"},
    {"Barrier probability", "Вероятность преград", "Barrierenwahrscheinlichkeit"},
    {"Unique rows and columns", "Разные ряды и столбцы", "Eindeutige Zeilen und Spalten"},
    {"Type of solid", "Тип тела", "Art des Körpers"},
    {"Strip clues", "Убрать лишние подсказки", "Überflüssige Hinweise entfernen"},
    {"Start and end in corners", "Начало и конец в углах", "Start und Ziel in Ecken"},
    {"Size (s*s)", "Размер (s*s)", "Größe (s*s)"},
    {"Shape type", "Тип формы", "Formtyp"},
    {"Scoring system", "Подсчёт очков", "Wertung"},
    {"Rows of sub-blocks", "Блоков по вертикали", "Blockzeilen"},
    {"Columns of sub-blocks", "Блоков по горизонтали", "Blockspalten"},
    {"Rotating block size", "Размер вращаемого блока", "Größe des Drehblocks"},
    {"Region size", "Размер области", "Regionsgröße"},
    {"Regions", "Области", "Regionen"},
    {"Pegs per guess", "Фишек в попытке", "Steine pro Versuch"},
    {"Orientation matters", "Важна ориентация", "Ausrichtung zählt"},
    {"One number per row", "Одно число в ряду", "Eine Zahl pro Reihe"},
    {"Number of points", "Число точек", "Anzahl Punkte"},
    {"No. of colours", "Число цветов", "Anzahl Farben"}, {"Colours", "Цвета", "Farben"},
    {"No. of balls", "Число шаров", "Anzahl Kugeln"},
    {"Multiplication only", "Только умножение", "Nur Multiplikation"},
    {"Maximum number on dominoes", "Наибольшее число на костях", "Höchste Zahl auf Dominosteinen"},
    {"Max. bridges per direction", "Макс. мостов в направлении", "Max. Brücken pro Richtung"},
    {"Killer (digit sums)", "Киллер (суммы цифр)", "Killer (Ziffernsummen)"},
    {"Jigsaw (irregularly shaped sub-blocks)", "Пазл (блоки неправильной формы)", "Puzzle (unregelmäßige Blöcke)"},
    {"Height / bottom", "Высота / низ", "Höhe / unten"}, {"Width / top", "Ширина / верх", "Breite / oben"},
    {"Guesses", "Попытки", "Versuche"}, {"Extra moves permitted", "Дополнительных ходов", "Zusätzliche Züge erlaubt"},
    {"Expansion factor (%age)", "Коэффициент расширения (%)", "Erweiterungsfaktor (%)"},
    {"Expansion factor", "Коэффициент расширения", "Erweiterungsfaktor"},
    {"Disallow consecutive 1 clues", "Без соседних подсказок 1", "Keine aufeinanderfolgenden 1er-Hinweise"},
    {"Allow loops", "Разрешить петли", "Schleifen erlauben"}, {"Allow duplicates", "Разрешить повторы", "Duplikate erlauben"},
    {"Allow blanks", "Разрешить пустые", "Leerfelder erlauben"},
    {"Aggressive generation (longer)", "Жёсткая генерация (дольше)", "Aggressive Erzeugung (länger)"},
    {"Grid type", "Тип сетки", "Gittertyp"}, {"Grid size", "Размер сетки", "Gittergröße"},
    {"Board type", "Тип поля", "Spielfeldtyp"}, {"Symmetry", "Симметрия", "Symmetrie"},
    {"Difficulty", "Сложность", "Schwierigkeit"}, {"Width", "Ширина", "Breite"}, {"Height", "Высота", "Höhe"},
    {"Mines", "Мины", "Minen"}, {"Mode", "Режим", "Modus"},
    {"Move the tile", "Двигать фишку", "Plättchen bewegen"}, {"Move the gap", "Двигать пустоту", "Lücke bewegen"},
    {"Cyclic", "По кругу", "Zyklisch"}, {"Each to white", "Каждая в белую", "Jedes auf weiß"},
    {"All to white", "Все в белые", "Alle auf weiß"},
    {"Left to fill, right to dot", "Левая: заливка, правая: точка", "Links füllen, rechts Punkt"},
    {"Left to dot, right to fill", "Левая: точка, правая: заливка", "Links Punkt, rechts füllen"},
    {"Unidirectional", "Односторонние", "Einseitig"}, {"Meshing gears", "Сцепленные шестерёнки", "Ineinandergreifende Zahnräder"},
    {"Based on grid only", "Только по сетке", "Nur nach Gitter"},
    {"Based on grid and game state", "По сетке и состоянию игры", "Nach Gitter und Spielstand"},
    {"Total", "Всего", "Gesamt"}, {"Remaining", "Осталось", "Verbleibend"}, {"Placed/Total", "Стоит/Всего", "Gesetzt/Gesamt"},
    {"Traditional", "Классический", "Klassisch"}, {"Loopy-style", "Как в Loopy", "Wie Loopy"},
    {"Circles", "Кружки", "Kreise"}, {"Numbers", "Числа", "Zahlen"}, {"Pictures", "Картинки", "Bilder"}, {"Letters", "Буквы", "Buchstaben"},
    {"Half-grid", "Полусетка", "Halbgitter"}, {"Full-grid", "Вся сетка", "Ganzes Gitter"},
    {"Cube", "Куб", "Würfel"}, {"None", "Нет", "Keine"}, {"No", "Нет", "Nein"},
    {"2-way mirror", "Зеркало 2", "2-fach Spiegel"}, {"4-way mirror", "Зеркало 4", "4-fach Spiegel"},
    {"8-way mirror", "Зеркало 8", "8-fach Spiegel"},
    {"2-way rotational", "Поворот 2", "2-fach Drehung"}, {"4-way rotational", "Поворот 4", "4-fach Drehung"},
    {"2-way rotation", "Поворот 2", "2-fach Drehung"}, {"4-way rotation", "Поворот 4", "4-fach Drehung"},
    {"2-way diagonal mirror", "Диагональное зеркало 2", "2-fach Diagonalspiegel"},
    {"4-way diagonal mirror", "Диагональное зеркало 4", "4-fach Diagonalspiegel"},
    /* status bar */
    {"COMPLETED!", "ГОТОВО!", "GELÖST!"}, {"Auto-solved.", "Решено автоматически.", "Automatisch gelöst."},
    {"Auto solved", "Решено автоматически", "Automatisch gelöst"},
    {"Auto-solver used.", "Использовано автореш.", "Automatik benutzt."},
    {"Moves since auto-solve:", "Ходов после автореш.:", "Züge nach Auto-Lösung:"},
    {"Moves:", "Ходы:", "Züge:"}, {"moves", "ходов", "Züge"}, {"Active:", "Активно:", "Aktiv:"},
    {"Balls marked:", "Шаров отмечено:", "Kugeln markiert:"},
    {"too many balls marked.", "отмечено слишком много шаров.", "zu viele Kugeln markiert."},
    {"wrong and", "неверных и", "falsch und"}, {"missed balls.", "пропущенных шаров.", "übersehene Kugeln."},
    {"Marked:", "Отмечено:", "Markiert:"}, {"Score:", "Счёт:", "Punkte:"}, {"Selected:", "Выбрано:", "Gewählt:"},
    {"Click button to verify guesses.", "Нажмите кнопку для проверки.", "Zum Prüfen Taste drücken."},
    {"Gems:", "Самоцветы:", "Edelsteine:"}, {"Deaths:", "Потери:", "Tode:"}, {"DEAD!", "ПОГИБ!", "TOT!"},
    {"Region size:", "Размер области:", "Regionsgröße:"}, {"Clues left:", "Подсказок осталось:", "Hinweise übrig:"},
    {"Nothing", "Ничего", "Nichts"},
    /* key labels of the on-screen keypad */
    {"Clear", "Сброс", "Entf."}, {"Backspace", "Стереть", "Entf."},
};

static const Gloss **sorted;
static int nsorted;

static int cmp_len(const void *a, const void *b)
{
    const Gloss *x = *(const Gloss *const *)a, *y = *(const Gloss *const *)b;
    return (int)strlen(y->en) - (int)strlen(x->en);   /* longest first */
}

static void gloss_init(void)
{
    int i, n = (int)(sizeof GLOSS / sizeof GLOSS[0]);
    if (sorted) return;
    sorted = malloc(n * sizeof *sorted);
    for (i = 0; i < n; i++) sorted[i] = &GLOSS[i];
    qsort(sorted, n, sizeof *sorted, cmp_len);
    nsorted = n;
}

static int is_alpha(int c) { return isalpha((unsigned char)c); }

const char *tr_glossary(const char *en, char *out, size_t n)
{
    int l = tr_lang();
    size_t o = 0;
    const char *p = en;
    if (l == LANG_EN) {
        snprintf(out, n, "%s", en);
        return out;
    }
    gloss_init();
    while (*p && o + 1 < n) {
        int i, hit = 0;
        for (i = 0; i < nsorted; i++) {
            const Gloss *g = sorted[i];
            size_t el = strlen(g->en);
            const char *rep;
            size_t rl;
            if (strncmp(p, g->en, el) != 0) continue;
            if (p > en && is_alpha(p[-1]) && is_alpha(g->en[0])) continue;
            if (is_alpha(p[el]) && is_alpha(g->en[el - 1])) continue;
            rep = l == LANG_RU ? g->ru : g->de;
            rl = strlen(rep);
            if (o + rl + 1 >= n) rl = n - o - 1;
            memcpy(out + o, rep, rl);
            o += rl;
            p += el;
            hit = 1;
            break;
        }
        if (!hit) out[o++] = *p++;
    }
    out[o] = 0;
    return out;
}
