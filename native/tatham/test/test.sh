#!/bin/sh
# Runs the real app (main.c, fe.c, tr.c, all 40 puzzles) on the host against test/fake_inkview.c:
# scripted taps, holds, drags, keys; checks screens, texts in 3 languages, saves, presets, custom settings.
# Usage: sh test/test.sh   (SAN=0 for a build without sanitizers)
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-/tmp/tatham-host-build}
export ASAN_OPTIONS=detect_leaks=0
sh "$HERE/test/build-host.sh" >/dev/null
BIN="$OUT/tatham-host"
DATA="$OUT/data"
SHOTS=${SHOTS:-$OUT/shots}
mkdir -p "$SHOTS"
run() { # name script-file
    printf '%-28s ' "$1"
    FAKE_SCRIPT="$2" "$BIN" || { echo "  script: $2"; exit 1; }
}
fresh() { rm -rf "$DATA"; printf 'language=en\n' > "$OUT/global.cfg"; }

# ---- 0. every puzzle has a Russian and German name and rules (slug = htmlhelp_topic) ----
python3 -I - "$HERE" <<'PY'
import re, sys, glob
here = sys.argv[1]
games = re.findall(r"GAME\((\w+)\)", open(here + "/puzzles/generated-games.h").read())
names = re.findall(r'^    \{"([^"]*)", "([^"]*)", "([^"]*)"\},$', open(here + "/tr.c", encoding="utf-8").read(), re.M)
rules = re.findall(r'^    \{"(\w+)", \{$', open(here + "/rules.c", encoding="utf-8").read(), re.M)
bad = 0
for g in games:
    src = open(f"{here}/puzzles/{g}.c", encoding="utf-8").read()
    m = re.search(r'(?:const struct game|struct game const|const game) thegame = \{\s*"([^"]+)", "[^"]*", "([^"]+)"', src)
    if not m: print("no game struct in", g); bad += 1; continue
    name, slug = m.groups()
    if name not in [n[0] for n in names]: print("no translation of the name", name); bad += 1
    if slug not in rules: print("no rules for", slug); bad += 1
for n in names:
    if not all(n): print("empty name translation", n); bad += 1
if len(games) != 40 or len(set(rules)) != 40: print("expected 40 games and 40 rules,", len(games), len(set(rules))); bad += 1
sys.exit(1 if bad else 0)
PY
echo "names and rules             ok (40 puzzles x 3 languages)"

# ---- 1. every puzzle: open, random play, rules, menu, solve, back; then everything is saved ----
fresh
NAMES=$(sed -n 's/^    {"\([^"]*\)", "[^"]*", "[^"]*"},$/\1/p' "$HERE/tr.c" | sed -n '1,40p')
S1="$OUT/s1.txt"
{
  echo "size 1072 1448"; echo "show"
  n=0
  echo "$NAMES" | while IFS= read -r name; do
    n=$((n+1))
    echo "tapfind $name"
    echo "expect Undo"
    echo "pgm $SHOTS/last.pgm"
    echo "expect_ink 0 220 1072 900 5 10000"
    echo "rtap 25 $n 30 230 1040 1000"
    echo "tapl Menu"
    echo "tapl Rules and controls"
    echo "expect Tip:"
    echo "tapl Back"
    echo "tapl Menu"
    echo "tapo Show solution"
    echo "tapo Menu"
    echo "tapo Games"
    echo "tapo Back"
    echo "tapo Games"
    echo "expect Puzzle Collection"
  done
  echo "pgm $SHOTS/home1.pgm"
  echo "quit"
} > "$S1"
run "40 puzzles end to end" "$S1"
[ "$(ls "$DATA"/*.sav | wc -l)" -eq 40 ] || { echo "expected 40 save files"; exit 1; }
echo "   40 save files written"

# ---- 2. restart: saved games are listed and resume ----
S2="$OUT/s2.txt"
cat > "$S2" <<EOT
size 1072 1448
show
expect in progress
tapfind Loopy
expect Undo
pgm $SHOTS/resumed.pgm
tapl Games
tapfind Solo
expect Undo
quit
EOT
run "resume saved games" "$S2"

# ---- 3. size and difficulty, custom settings, confirm dialog, thick lines, hardware keys ----
fresh
S3="$OUT/s3.txt"
cat > "$S3" <<EOT
size 1072 1448
show
tapfind Solo
tapl Menu
tapl Size and difficulty
expect Basic
pgm $SHOTS/presets.pgm
tapl Next >
expect Jigsaw
tapl < Prev
tapfind Custom...
expect Custom settings
expect_not Колонки
tapl Cancel
expect Undo
tapl Games
tapfind Fifteen
tapl Menu
tapl Size and difficulty
tapfind Custom...
expect Width: 4
tapl Width: 4
kbtitle Width
kbtype 3
expect Width: 3
tapl OK
expect Moves: 0
expect Fifteen
rtap 6 3 40 230 1030 1000
tapl New
expect Start a new puzzle?
tapl No
expect Undo
tapl New
tapl Yes
expect Moves: 0
tapl Menu
tapl Thick lines: off
expect Thick lines: on
tapl Back
tapl Menu
tapl Restart this puzzle
expect Undo
key 24
key 25
key 27
expect Puzzle Collection
key 23
expect Language
tapl Back
quit
EOT
run "presets, custom, dialogs" "$S3"

# ---- 4. Russian and German: UI, puzzle names, rules, presets, option labels ----
fresh
S4="$OUT/s4.txt"
cat > "$S4" <<EOT
size 1072 1448
show
tapl Menu
tapl Language
tapl Русский
expect Коллекция головоломок
expect Сапёр
tapfind Сапёр
expect Отмена
expect Новая
expect Игры
tapl Меню
expect Правила и управление
tapl Размер и сложность
expect мин
tapl Назад
expect Отмена
tapl Меню
tapl Правила и управление
expect Подсказка:
tapl Назад
tapl Меню
tapl Игры
tapl Меню
tapl Язык
tapl Deutsch
expect Rätselsammlung
tapfind Minensuche
expect Zurück
expect Neu
tapl Menü
tapl Regeln und Bedienung
expect Tipp:
tapl Zurück
tapl Menü
tapl Größe und Schwierigkeit
expect Minen
tapfind Eigene Werte...
expect Eigene Werte
expect Breite
tapl Abbrechen
tapl Menü
tapl Spiele
tapl Menü
tapl Sprache
tapl Systemstandard
expect Puzzle Collection
quit
EOT
run "Russian and German" "$S4"
grep -q '^lang=system' "$DATA/tatham.cfg" || { echo "language setting not saved"; exit 1; }
printf 'language=ru\n' > "$OUT/global.cfg"
S5="$OUT/s5.txt"
cat > "$S5" <<EOT
size 1072 1448
show
expect Коллекция головоломок
quit
EOT
run "system language = ru" "$S5"

# ---- 5. other screen sizes and landscape ----
fresh
S6="$OUT/s6.txt"
cat > "$S6" <<EOT
size 600 800
show
tapfind Solo
expect Undo
rtap 20 4 20 130 580 520
orient 1
expect Undo
pgm $SHOTS/landscape.pgm
orient 0
expect Undo
quit
EOT
run "600x800 and rotation" "$S6"

# ---- 6. no text is cut off, in every language (real Liberation metrics when FreeType is installed) ----
trunc() { # (TW, TH = screen size) lang next menu games size custom cancel rules back solve train mines solo
    fresh
    printf 'language=%s\n' "$1" > "$OUT/global.cfg"
    S7="$OUT/s7-$1-$TW.txt"
    cat > "$S7" <<EOT
size $TW $TH
show
expect_no_trunc
tapl $2
expect_no_trunc
tapfind ${11}
expect_no_trunc
tapl $3
expect_no_trunc
tapl $5
expect_no_trunc
tapfind $6
expect_no_trunc
tapl $7
expect_no_trunc
tapl $3
tapl $8
expect_no_trunc
tapl $9
tapl $3
tapl ${10}
expect_no_trunc
tapl $4
tapfind ${12}
expect_no_trunc
tapl $3
tapl $5
expect_no_trunc
tapl $2
expect_no_trunc
tapl $2
expect_no_trunc
tapl $9
tapl $4
tapfind ${13}
expect_no_trunc
pgm $SHOTS/fit-$1-$TW.pgm
quit
EOT
    run "text fits ($1, ${TW}x$TH)" "$S7"
}
for TW_TH in "1072 1448" "600 800"; do   # the PB634 and the PB625 (sizes scale with the width)
    TW=${TW_TH% *}; TH=${TW_TH#* }
    trunc en "Next >" Menu Games "Size and difficulty" "Custom..." Cancel "Rules and controls" Back "Show solution" "Train Tracks" Mines Solo
    trunc ru "Далее >" Меню Игры "Размер и сложность" "Свои параметры..." Отмена "Правила и управление" Назад "Показать решение" "Железная дорога" Сапёр Судоку
    trunc de "Weiter >" Menü Spiele "Größe und Schwierigkeit" "Eigene Werte..." Abbrechen "Regeln und Bedienung" Zurück "Lösung zeigen" Gleise Minensuche Sudoku
done

echo "all tests passed"
