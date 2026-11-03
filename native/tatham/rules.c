/* Short rules for each puzzle, in English, Russian and German (key = the engine's htmlhelp_topic).
 * Based on the objectives in Simon Tatham's manual, shortened. */
#include <string.h>

#include "tr.h"

typedef struct { const char *slug, *t[NUM_LANGS]; } Rules;

static const Rules RULES[] = {
    {"blackbox", {
        "Find the balls hidden in the box. Fire beams in from the edge and see where they come out: "
        "a beam that hits a ball is absorbed, one that passes next to a ball is bent. Mark where you think "
        "the balls are, then press the check button.",
        "Найдите шары, спрятанные в коробке. Посылайте лучи с краёв и смотрите, где они выходят: "
        "луч, попавший в шар, поглощается, а рядом с шаром отклоняется. Отметьте, где, по-вашему, шары, "
        "и нажмите кнопку проверки.",
        "Finde die im Kasten versteckten Kugeln. Schicke Strahlen vom Rand hinein und beobachte, wo sie austreten: "
        "Ein Strahl, der eine Kugel trifft, wird verschluckt, neben einer Kugel wird er abgelenkt. Markiere die "
        "vermuteten Kugeln und drücke die Prüftaste."}},
    {"bridges", {
        "Connect all islands into one network with horizontal and vertical bridges. A number says how many "
        "bridges end on that island. At most two bridges join a pair of islands and bridges may not cross. "
        "Drag from island to island; drag again to add a second bridge.",
        "Соедините все острова в одну сеть горизонтальными и вертикальными мостами. Число на острове — сколько мостов "
        "к нему ведёт. Между двумя островами не больше двух мостов, мосты не пересекаются. Проведите пальцем "
        "от острова к острову; ещё раз — второй мост.",
        "Verbinde alle Inseln mit waagerechten und senkrechten Brücken zu einem Netz. Die Zahl nennt die Brücken "
        "an dieser Insel. Zwischen zwei Inseln höchstens zwei Brücken, Brücken kreuzen sich nicht. "
        "Zieh von Insel zu Insel; nochmal für eine zweite Brücke."}},
    {"cube", {
        "Roll the cube around the grid. When it rolls onto a blue square the square sticks to the bottom face; "
        "rolling a blue face onto an empty square drops it. Collect all blue squares onto the faces of the cube.",
        "Катайте куб по полю. Когда куб выкатывается на синюю клетку, она прилипает к нижней грани; синяя грань, "
        "выкаченная на пустую клетку, отпадает. Соберите все синие клетки на грани куба.",
        "Rolle den Würfel über das Feld. Rollt er auf ein blaues Feld, haftet es an der Unterseite; "
        "eine blaue Seite auf einem leeren Feld fällt ab. Sammle alle blauen Felder auf den Würfelseiten."}},
    {"dominosa", {
        "Pair up the numbered squares into dominoes so that every domino of the set appears exactly once. "
        "Tap between two squares to place a domino.",
        "Разбейте пронумерованные клетки на кости домино так, чтобы каждая кость набора встретилась ровно один раз. "
        "Нажмите между двумя клетками, чтобы положить кость.",
        "Fasse die Zahlenfelder zu Dominosteinen zusammen, so dass jeder Stein des Satzes genau einmal vorkommt. "
        "Tippe zwischen zwei Felder, um einen Stein zu legen."}},
    {"fifteen", {
        "Slide the numbered tiles into order, left to right and top to bottom, using the one empty space.",
        "Передвигайте фишки с числами в порядке слева направо и сверху вниз, используя единственную пустую клетку.",
        "Schiebe die nummerierten Plättchen mit Hilfe des einen freien Feldes in die Reihenfolge links nach rechts, oben nach unten."}},
    {"filling", {
        "Fill every square with a number so that each group of connected squares with the same number "
        "contains exactly that many squares (a group of 3s has three squares).",
        "Заполните клетки числами так, чтобы каждая связная область из одинаковых чисел состояла ровно из "
        "стольких клеток, каково это число (область из троек — три клетки).",
        "Fülle jedes Feld mit einer Zahl, so dass jede zusammenhängende Gruppe gleicher Zahlen genau so viele "
        "Felder hat, wie die Zahl angibt (eine Gruppe aus 3en hat drei Felder)."}},
    {"flip", {
        "Turn all squares white. Tapping a square flips it and the neighbours shown by the pattern. "
        "Work out the right combination of taps.",
        "Сделайте все клетки белыми. Нажатие переворачивает клетку и её соседей по заданному шаблону. "
        "Найдите нужную комбинацию нажатий.",
        "Mache alle Felder weiß. Ein Tipp dreht das Feld und die Nachbarn nach dem Muster um. "
        "Finde die richtige Kombination."}},
    {"flood", {
        "Make the whole grid one colour. Each move changes the colour of the flooded region in the top left corner. "
        "Do it within the move limit.",
        "Сделайте всё поле одного цвета. Каждый ход меняет цвет залитой области в левом верхнем углу. "
        "Уложитесь в лимит ходов.",
        "Mache das ganze Feld einfarbig. Jeder Zug ändert die Farbe des gefluteten Bereichs oben links. "
        "Schaffe es innerhalb des Zuglimits."}},
    {"galaxies", {
        "Divide the grid into galaxies. Every galaxy has a dot at its centre and is symmetric under a half turn around it; "
        "every square belongs to exactly one galaxy. Tap an edge to draw or erase a wall.",
        "Разделите поле на галактики. У каждой галактики есть точка-центр, и она симметрична при повороте на "
        "180° вокруг неё; каждая клетка принадлежит ровно одной галактике. Нажмите на границу, чтобы поставить или убрать стену.",
        "Teile das Feld in Galaxien. Jede hat einen Punkt als Mitte und ist bei einer halben Drehung darum "
        "symmetrisch; jedes Feld gehört zu genau einer Galaxie. Tippe auf eine Kante, um eine Wand zu setzen oder zu löschen."}},
    {"guess", {
        "Break the hidden colour code. Fill a row with pegs and press the check button: a black mark means a right colour "
        "in the right place, a white mark a right colour in the wrong place.",
        "Разгадайте скрытый цветной код. Заполните ряд фишками и нажмите проверку: чёрная метка — верный цвет "
        "на верном месте, белая — верный цвет не на своём месте.",
        "Knacke den versteckten Farbcode. Fülle eine Reihe mit Steinen und drücke die Prüftaste: schwarz = richtige Farbe "
        "am richtigen Platz, weiß = richtige Farbe am falschen Platz."}},
    {"inertia", {
        "Slide the ball around the grid: it keeps moving until something stops it. Collect all the gems "
        "and avoid the mines.",
        "Двигайте шар по полю: он катится, пока что-нибудь его не остановит. Соберите все самоцветы и обходите мины.",
        "Schiebe die Kugel über das Feld: Sie rollt, bis etwas sie stoppt. Sammle alle Edelsteine und meide die Minen."}},
    {"keen", {
        "Fill the grid with the digits 1 to n so that no digit repeats in a row or column. Each outlined cage "
        "shows a target number and an operation for the digits inside it.",
        "Заполните поле цифрами от 1 до n так, чтобы они не повторялись в строках и столбцах. В каждой обведённой "
        "клетке-группе указан результат и действие над её цифрами.",
        "Fülle das Feld mit den Ziffern 1 bis n, ohne Wiederholung in Zeilen und Spalten. Jeder umrandete Käfig "
        "nennt ein Ergebnis und eine Rechenart für seine Ziffern."}},
    {"lightup", {
        "Place lamps so that every empty square is lit. A lamp lights its row and column until a black square blocks it. "
        "Lamps must not light each other; a number on a black square says how many lamps touch it.",
        "Расставьте лампы так, чтобы освещена была каждая пустая клетка. Лампа освещает свой ряд и столбец до чёрной клетки. "
        "Лампы не должны освещать друг друга; число на чёрной клетке — сколько ламп к ней прилегает.",
        "Setze Lampen so, dass jedes leere Feld beleuchtet ist. Eine Lampe leuchtet Zeile und Spalte bis zu einem schwarzen Feld aus. "
        "Lampen dürfen sich nicht gegenseitig beleuchten; die Zahl auf einem schwarzen Feld nennt die angrenzenden Lampen."}},
    {"loopy", {
        "Draw a single closed loop along the dotted lines. A number in a cell says how many of its edges are part of the loop.",
        "Проведите одну замкнутую петлю по пунктирным линиям. Число в клетке — сколько её сторон входит в петлю.",
        "Zeichne eine einzige geschlossene Schleife entlang der Punktlinien. Die Zahl in einem Feld nennt, wie viele "
        "seiner Kanten zur Schleife gehören."}},
    {"magnets", {
        "Fill the grid with magnets, each covering two squares with a + and a - end, or leave squares empty. "
        "Numbers at the edge count the + and - in each row or column; the same poles may never touch.",
        "Заполните поле магнитами из двух клеток с концами + и − или оставьте клетки пустыми. Числа по краям — "
        "сколько + и − в ряду или столбце; одинаковые полюса не должны соприкасаться.",
        "Fülle das Feld mit Magneten aus zwei Feldern mit + und − oder lass Felder leer. Zahlen am Rand zählen + und − "
        "in Zeile oder Spalte; gleiche Pole dürfen sich nie berühren."}},
    {"map", {
        "Colour the map with four colours so that no two regions sharing a border have the same colour.",
        "Раскрасьте карту четырьмя цветами так, чтобы соседние области не были одного цвета.",
        "Färbe die Karte mit vier Farben, so dass keine zwei benachbarten Regionen dieselbe Farbe haben."}},
    {"mines", {
        "Uncover every square that is not a mine. A number tells how many mines touch that square. "
        "Mark suspected mines with the right button. The first tap is always safe.",
        "Откройте все клетки без мин. Число показывает, сколько мин рядом. Подозрительные клетки отмечайте "
        "правой кнопкой. Первое нажатие всегда безопасно.",
        "Decke alle Felder ohne Mine auf. Eine Zahl nennt die benachbarten Minen. Markiere vermutete Minen mit der "
        "rechten Taste. Der erste Tipp ist immer sicher."}},
    {"mosaic", {
        "Colour squares black or white. Each number says how many black squares are in its 3x3 neighbourhood, "
        "counting the square itself.",
        "Закрасьте клетки чёрным или белым. Число — сколько чёрных клеток в квадрате 3x3 вокруг него, включая саму клетку.",
        "Färbe Felder schwarz oder weiß. Jede Zahl nennt die schwarzen Felder in ihrer 3x3-Umgebung einschließlich "
        "des Feldes selbst."}},
    {"net", {
        "Rotate the tiles so that all of them connect to the central source with no loose ends and no loops. "
        "Left button turns one way, right button the other; a long press on a tile locks it.",
        "Поверните плитки так, чтобы все они соединились с центральным источником без свободных концов и петель. "
        "Левая кнопка поворачивает в одну сторону, правая в другую.",
        "Drehe die Teile so, dass alle mit der Quelle in der Mitte verbunden sind, ohne lose Enden und Schleifen. "
        "Linke Taste dreht in die eine, rechte in die andere Richtung."}},
    {"netslide", {
        "Like Net, but instead of rotating pieces you slide whole rows and columns. "
        "Connect everything to the source without loops.",
        "Как «Сеть», но вместо поворота плиток вы сдвигаете целые ряды и столбцы. Соедините всё с источником без петель.",
        "Wie Netz, aber statt zu drehen schiebst du ganze Zeilen und Spalten. Verbinde alles mit der Quelle ohne Schleifen."}},
    {"palisade", {
        "Divide the grid into regions of the given size by drawing walls. A number says how many of that square's "
        "four sides are walls.",
        "Разделите поле стенами на области заданного размера. Число — сколько из четырёх сторон клетки являются стенами.",
        "Teile das Feld durch Wände in Regionen der angegebenen Größe. Eine Zahl nennt, wie viele der vier Seiten "
        "des Feldes Wände sind."}},
    {"pattern", {
        "Nonogram: colour squares black so that the runs of black squares in every row and column match the numbers "
        "at the edge. Right button marks a square as empty.",
        "Нонограмма: закрасьте клетки так, чтобы группы чёрных клеток в каждой строке и столбце совпали с числами "
        "по краям. Правая кнопка отмечает пустую клетку.",
        "Nonogramm: Färbe Felder schwarz, so dass die Blöcke schwarzer Felder in jeder Zeile und Spalte zu den Zahlen "
        "am Rand passen. Die rechte Taste markiert ein leeres Feld."}},
    {"pearl", {
        "Draw one closed loop through every pearl. The loop turns on a black pearl and goes straight on both sides of "
        "it; it goes straight through a white pearl and turns just before or after it.",
        "Проведите одну замкнутую петлю через все жемчужины. На чёрной жемчужине петля поворачивает и по обе стороны идёт "
        "прямо; через белую идёт прямо, а до или после неё поворачивает.",
        "Zeichne eine geschlossene Schleife durch alle Perlen. An einer schwarzen Perle biegt sie ab und läuft beidseitig "
        "geradeaus; durch eine weiße läuft sie gerade und biegt davor oder danach ab."}},
    {"pegs", {
        "Peg solitaire: jump a peg over a neighbouring peg into an empty hole; the jumped peg is removed. "
        "Leave as few pegs as possible, ideally one.",
        "Солитёр: перепрыгивайте фишкой через соседнюю в пустую лунку, перепрыгнутая фишка снимается. "
        "Оставьте как можно меньше фишек, в идеале одну.",
        "Steckhalma: Springe mit einem Stein über einen Nachbarn in ein leeres Loch; der übersprungene Stein wird entfernt. "
        "Lass möglichst wenige, am besten einen Stein übrig."}},
    {"range", {
        "Colour some squares black. Each number says how many squares, counting itself, you can see from it in a straight line "
        "before a black square. Black squares never touch edge to edge and white squares stay connected.",
        "Закрасьте некоторые клетки чёрным. Число — сколько клеток (считая её саму) видно из неё по прямой до чёрной клетки. "
        "Чёрные клетки не соприкасаются сторонами, а белые остаются связными.",
        "Färbe einige Felder schwarz. Jede Zahl nennt die Felder (sie selbst eingerechnet), die man von ihr geradeaus bis zu einem "
        "schwarzen Feld sieht. Schwarze Felder berühren sich nicht seitlich, weiße bleiben verbunden."}},
    {"rect", {
        "Divide the grid into rectangles. Every rectangle contains exactly one number, equal to its area.",
        "Разделите поле на прямоугольники. В каждом ровно одно число, равное его площади.",
        "Teile das Feld in Rechtecke. Jedes enthält genau eine Zahl, die seiner Fläche entspricht."}},
    {"samegame", {
        "Remove groups of two or more touching tiles of the same colour. Bigger groups score more. "
        "Clear the board, or get as high a score as you can.",
        "Убирайте группы из двух и более соприкасающихся плиток одного цвета. Чем больше группа, тем больше очков. "
        "Очистите поле или наберите максимум очков.",
        "Entferne Gruppen aus zwei oder mehr zusammenhängenden Steinen gleicher Farbe. Größere Gruppen geben mehr Punkte. "
        "Räume das Feld ab oder erreiche eine möglichst hohe Punktzahl."}},
    {"signpost", {
        "Draw one path through all squares from the first to the last. Each arrow points to the next square of the path; "
        "some squares have their step number given.",
        "Проведите один путь через все клетки от первой до последней. Каждая стрелка указывает на следующую клетку пути; "
        "у некоторых клеток номер шага уже задан.",
        "Zeichne einen Weg durch alle Felder vom ersten zum letzten. Jeder Pfeil zeigt auf das nächste Feld des Weges; "
        "bei manchen Feldern ist die Schrittnummer gegeben."}},
    {"singles", {
        "Black out squares so that no number appears twice among the white squares of any row or column. "
        "Black squares never touch edge to edge and the white squares stay connected.",
        "Закрасьте клетки так, чтобы среди белых клеток любой строки и столбца ни одно число не повторялось. "
        "Чёрные клетки не соприкасаются сторонами, белые остаются связными.",
        "Schwärze Felder so, dass unter den weißen Feldern jeder Zeile und Spalte keine Zahl doppelt vorkommt. "
        "Schwarze Felder berühren sich nicht seitlich, die weißen bleiben verbunden."}},
    {"sixteen", {
        "Slide whole rows and columns (they wrap around) to put the numbered tiles in order.",
        "Сдвигайте целые ряды и столбцы (они замкнуты в кольцо), чтобы расставить плитки с числами по порядку.",
        "Schiebe ganze Zeilen und Spalten (sie laufen im Kreis), um die Plättchen in Reihenfolge zu bringen."}},
    {"slant", {
        "Put a diagonal in every square, slash or backslash. A number at a corner says how many diagonals touch that point. "
        "The diagonals must not form a closed loop.",
        "В каждой клетке проведите диагональ — / или \\. Число в углу — сколько диагоналей сходится в этой точке. "
        "Диагонали не должны образовывать замкнутых петель.",
        "Setze in jedes Feld eine Diagonale, / oder \\. Eine Zahl an einer Ecke nennt, wie viele Diagonalen dort enden. "
        "Die Diagonalen dürfen keine geschlossene Schleife bilden."}},
    {"solo", {
        "Sudoku: fill the grid with digits so that none repeats in a row, a column or a block. "
        "Variants add irregular blocks (jigsaw) or cages with sums (killer). Use the keypad; right button writes pencil marks.",
        "Судоку: заполните поле цифрами так, чтобы они не повторялись в строке, столбце и блоке. В вариантах "
        "блоки неправильной формы (пазл) или клетки с суммами (киллер). Кнопки цифр внизу; правая кнопка — пометки карандашом.",
        "Sudoku: Fülle das Feld mit Ziffern, die sich in Zeile, Spalte und Block nicht wiederholen. Varianten haben "
        "unregelmäßige Blöcke (Puzzle) oder Summenkäfige (Killer). Ziffern unten; rechte Taste = Bleistiftnotizen."}},
    {"tents", {
        "Put a tent next to every tree (up, down, left or right). Tents never touch each other, not even diagonally. "
        "Numbers at the edge say how many tents are in each row or column.",
        "Поставьте палатку рядом с каждым деревом (сверху, снизу, слева или справа). Палатки не соприкасаются, даже по диагонали. "
        "Числа по краям — число палаток в ряду или столбце.",
        "Stelle neben jeden Baum ein Zelt (oben, unten, links oder rechts). Zelte berühren sich nie, auch nicht diagonal. "
        "Zahlen am Rand nennen die Zelte je Zeile oder Spalte."}},
    {"towers", {
        "Fill the grid with towers of height 1 to n, no height repeating in a row or column. A number at the edge says how many "
        "towers you can see from there; taller towers hide shorter ones.",
        "Заполните поле башнями высотой от 1 до n, высоты не повторяются в строке и столбце. Число по краю — сколько башен "
        "видно с этой стороны; высокие закрывают низкие.",
        "Fülle das Feld mit Türmen der Höhe 1 bis n, keine Höhe doppelt in Zeile oder Spalte. Eine Zahl am Rand nennt, wie viele "
        "Türme man von dort sieht; höhere verdecken niedrigere."}},
    {"tracks", {
        "Lay one railway track from entrance to exit. Numbers at the edge say how many squares of each row and column "
        "contain track. The track never touches or crosses itself.",
        "Проложите один железнодорожный путь от входа до выхода. Числа по краям — сколько клеток в каждом ряду и столбце "
        "занято путём. Путь не касается и не пересекает сам себя.",
        "Lege ein einziges Gleis vom Eingang zum Ausgang. Zahlen am Rand nennen die Felder mit Gleis je Zeile und Spalte. "
        "Das Gleis berührt und kreuzt sich nie selbst."}},
    {"twiddle", {
        "Rotate small blocks of tiles, clockwise with the left button and anticlockwise with the right, until the numbers are in order.",
        "Вращайте небольшие блоки плиток — левой кнопкой по часовой стрелке, правой против — пока числа не встанут по порядку.",
        "Drehe kleine Blöcke von Plättchen, mit der linken Taste im Uhrzeigersinn, mit der rechten dagegen, bis die Zahlen geordnet sind."}},
    {"undead", {
        "Fill the grid with ghosts, vampires and zombies in the given numbers. Numbers at the edge say how many monsters you see "
        "along that line: vampires are invisible in mirrors, ghosts only visible in mirrors, zombies always.",
        "Расселите призраков, вампиров и зомби в заданных количествах. Числа по краям — сколько монстров видно вдоль линии: "
        "вампиры не видны в зеркалах, призраки видны только в зеркалах, зомби видны всегда.",
        "Verteile Geister, Vampire und Zombies in den angegebenen Anzahlen. Zahlen am Rand nennen die sichtbaren Monster "
        "entlang der Linie: Vampire sind im Spiegel unsichtbar, Geister nur im Spiegel sichtbar, Zombies immer."}},
    {"unequal", {
        "Fill the grid with the digits 1 to n, none repeating in a row or column, so that the inequality signs between squares hold "
        "(or, in the Adjacent variant, so that neighbours differing by one are marked).",
        "Заполните поле цифрами от 1 до n без повторов в строках и столбцах, чтобы выполнялись знаки неравенства между "
        "клетками (в варианте Adjacent — чтобы были отмечены соседи, отличающиеся на единицу).",
        "Fülle das Feld mit den Ziffern 1 bis n ohne Wiederholung in Zeilen und Spalten, so dass die Ungleichheitszeichen "
        "stimmen (bei Adjacent: so dass Nachbarn mit Unterschied eins markiert sind)."}},
    {"unruly", {
        "Fill the grid with black and white squares: no more than two of the same colour in a row, each row and column has "
        "equally many of each, and no two rows or columns are identical.",
        "Заполните поле чёрными и белыми клетками: не больше двух одного цвета подряд, в каждой строке и столбце поровну "
        "обоих цветов, и никакие две строки или два столбца не совпадают.",
        "Fülle das Feld mit schwarzen und weißen Feldern: höchstens zwei gleiche nebeneinander, in jeder Zeile und Spalte gleich "
        "viele von jeder Farbe, und keine zwei Zeilen oder Spalten sind gleich."}},
    {"untangle", {
        "Move the points so that none of the connecting lines cross each other.",
        "Передвиньте точки так, чтобы ни одна из соединяющих линий не пересекалась с другой.",
        "Verschiebe die Punkte so, dass sich keine der Verbindungslinien kreuzt."}},
};

const char *tr_rules(const char *slug)
{
    size_t i;
    for (i = 0; i < sizeof RULES / sizeof RULES[0]; i++)
        if (strcmp(RULES[i].slug, slug) == 0) return RULES[i].t[tr_lang()];
    return "";
}
