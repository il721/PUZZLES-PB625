#!/bin/sh
# Runs the Puzzle Book shell and the Square Rebuses and Digit Rebuses modules (native/puzzlebook) on the build host against tools/fake-inkview (SVG output) and plays
# every puzzle through the on-screen buttons, portrait and landscape. Prints PASS/FAIL.
# Optional: OUT=dir keeps the SVG screenshots (and PNGs if playwright is installed).
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
OUT=${OUT:-$T}
mkdir -p "$OUT"
N=native/puzzlebook
gcc -Wall -Wextra -std=gnu99 -Itools/fake-inkview -c tools/fake-inkview/inkview_svg.c -o "$T/shim.o"
for f in $N/shell.c $N/ui.c $N/i18n.c $N/progress.c tools/test-modules1.c $N/rebus/module_rebus.c $N/rebus/rebus_core.c; do
  gcc -Wall -Wextra -std=gnu99 -Dmain=rebus_main -DSAVEF="\"$T/rebus.sav\"" -DLOGF="\"$T/log.txt\"" -DCFGF="\"$T/cfg\"" -DSYSCFGF="\"$T/global.cfg\"" -DPROGF="\"$T/progress.json\"" \
    -Itools/fake-inkview -I$N -I$N/rebus -c $f -o "$T/$(basename $f .c).o"
done
gcc "$T"/*.o -o "$T/ui"

# second variant: two modules (template module registered through tools/test-modules2.c)
T2="$T/two"; mkdir -p "$T2"
for f in $N/shell.c $N/ui.c $N/i18n.c $N/progress.c tools/test-modules2.c $N/rebus/module_rebus.c $N/rebus/rebus_core.c $N/example/module_example.c; do
  sv=rebus.sav; case $f in *example*) sv=example.sav;; esac
  gcc -Wall -Wextra -std=gnu99 -Dmain=rebus_main -DSAVEF="\"$T/$sv\"" -DLOGF="\"$T/log.txt\"" -DCFGF="\"$T/cfg\"" -DSYSCFGF="\"$T/global.cfg\"" -DPROGF="\"$T/progress.json\"" \
    -Itools/fake-inkview -I$N -I$N/rebus -c $f -o "$T2/$(basename $f .c).o"
done
gcc "$T/shim.o" "$T2"/*.o -o "$T/ui2"

# third variant: the real registry (modules.c): Square Rebuses + Digit Rebuses
T3="$T/full"; mkdir -p "$T3"
for f in $N/shell.c $N/ui.c $N/i18n.c $N/progress.c $N/modules.c $N/rebus/module_rebus.c $N/rebus/rebus_core.c $N/digit/module_digit.c $N/digit/digit_core.c; do
  gcc -Wall -Wextra -std=gnu99 -Dmain=rebus_main -DSAVEF="\"$T/rebus.sav\"" -DDIGITSAVEF="\"$T/digit.sav\"" -DLOGF="\"$T/log.txt\"" -DCFGF="\"$T/cfg\"" -DSYSCFGF="\"$T/global.cfg\"" -DPROGF="\"$T/progress.json\"" \
    -Itools/fake-inkview -I$N -I$N/rebus -I$N/digit -c $f -o "$T3/$(basename $f .c).o"
done
gcc "$T/shim.o" "$T3"/*.o -o "$T/ui3"

python3 - "$T" "$OUT" <<'PY'
import json, sys
T, OUT = sys.argv[1:3]
data = json.load(open('native/puzzlebook/rebus/module01.json'))['puzzles']

def solve_script(W, H, tag):
    L = ['size %d %d' % (W, H), 'show', 'expect Puzzle Book', 'expect 0 / 15 solved', 'svg %s/%s-home.svg' % (OUT, tag),
         'tapl Square Rebuses', 'expect Puzzle 1/15', 'svg %s/%s-start.svg' % (OUT, tag)]
    # start is p01 (cur=1); go through all puzzles with wrap-around: p01..p15, then Example
    order = list(range(1, len(data))) + [0]
    for n, i in enumerate(order):
        p = data[i]
        given = {(g[0], g[1], g[2]) for g in p['givens']}
        def num(r, s):
            return p['total'] if (r, s) == (4, 4) else p['rows'][s]['result'] if r == 4 else (p['rows'][r]['nums'][s] if s < 4 else p['rows'][r]['result'])
        # wrong digit first, Check must flag it, then fix via Del + right digit
        cells = [(r, s, k) for r in range(5) for s in range(5) for k in range(len(num(r, s))) if (r, s, k) not in given]
        for c in cells:
            L.append('tapl %s' % num(*c[:2])[c[2]])
        L.append('expect Solved!')
        if i == 3:
            L.append('svg %s/%s-solved.svg' % (OUT, tag))
        if n < len(order) - 1:
            L.append('tapl >')
    L += ['tapl List', 'expect choose a puzzle', 'svg %s/%s-list.svg' % (OUT, tag), 'tapl Back',
          'tapl ?', 'expect How to play', 'svg %s/%s-help.svg' % (OUT, tag), 'key 27', 'expect Example',
          # mistakes: first digit twice wrong then Check
          'tapl <', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9', 'tapl 9',
          'tapl Check', 'expect broken', 'svg %s/%s-errors.svg' % (OUT, tag),
          'tapl Reset', 'expect Tap Reset again', 'tapl Reset', 'tapl Check', 'expect No mistakes',
          # back to the shell: counts, random puzzle (only p15 is unsolved), help, Back key closes the app
          'tapl Home', 'expect 14 / 15 solved', 'tapl Random', 'expect Puzzle 15/15',
          # the Home button on the list screen goes to the main menu
          'tapl List', 'tapl Home', 'expect 14 / 15 solved', 'tapl Random',
          # hardware Home/Menu key: from the puzzle, from the list and from the help straight to the home screen
          'key 23', 'expect 14 / 15 solved', 'tapl Random', 'tapl List', 'key 23', 'expect 14 / 15 solved', 'tapl Random', 'tapl ?', 'key 26', 'expect 14 / 15 solved', 'tapl Random', 'tapl Home',
          'tapl ?', 'expect Puzzle Book - help', 'tap 300 400', 'expect Random', 'key 27', 'expect_closed']
    return '\n'.join(L) + '\n'

for W, H, tag in ((600, 800, 'portrait'), (800, 600, 'landscape')):
    open('%s/%s.script' % (T, tag), 'w').write(solve_script(W, H, tag))
    open('%s/%s-random.script' % (T, tag), 'w').write('size %d %d\nshow\ntapl Random\nexpect Puzzle \ntapl Home\nexpect 0 / 15 solved\nquit\n' % (W, H))
PY

RC=0
for tag in portrait landscape; do
  rm -f "$T/rebus.sav"
  if REBUS_SCRIPT="$T/$tag-random.script" "$T/ui"; then echo "PASS random $tag"; else echo "FAIL random $tag"; RC=1; fi
  rm -f "$T/rebus.sav"
  if REBUS_SCRIPT="$T/$tag.script" "$T/ui"; then echo "PASS ui $tag"; else echo "FAIL ui $tag"; RC=1; fi
done
rm -f "$T/cfg" "$T/global.cfg"
# the save file written by the last single-module run: 15 solved lines (14 puzzles + the sample)
grep -c '^1 ' "$T/rebus.sav" | sed 's/^/solved puzzles saved: /'
cat > "$T/two.script" <<EOS
size 600 800
show
expect Square Rebuses
expect Example module
svg $OUT/two-home.svg
tapl Example module
expect Tap anywhere to finish.
tap 300 400
expect Done!
tapl Home
expect 1 / 1 solved
tapl Square Rebuses
expect Puzzle 1/15
key 27
expect 1 / 1 solved
key 27
expect_closed
EOS
rm -f "$T/rebus.sav"
if REBUS_SCRIPT="$T/two.script" "$T/ui2"; then echo "PASS two modules"; else echo "FAIL two modules"; RC=1; fi

# ---- Digit Rebuses (second real module, real registry): solve all 19 puzzles through the buttons, portrait and landscape
python3 - "$T" "$OUT" <<'PY'
import json, sys
T, OUT = sys.argv[1:3]
data = json.load(open('native/puzzlebook/digit/module02.json'))['puzzles']
SETS = {'G1': (0, 8, 9), 'G2': (0, 6, 8), 'G3': (1, 4, 7), 'G4': (2, 3), 'G5': (3, 5, 9)}

def line_ok(cells, ops):
    acc = cells[0]
    for i, op in enumerate(ops):
        n = cells[i + 1]
        if op == '+': acc += n
        elif op == '-':
            acc -= n
            if acc < 0: return False
        elif op == '*': acc *= n
        else:
            if n == 0 or acc % n: return False
            acc //= n
    return acc == cells[3]

def broken(p, g):
    return any(not line_ok(g[r], p['rowOps'][r]) for r in range(4)) or \
           any(not line_ok([g[r][c] for r in range(4)], p['colOps'][c]) for c in range(4))

def script(W, H, tag):
    L = ['size %d %d' % (W, H), 'show', 'expect 0 / 18 solved', 'svg %s/%s-full-home.svg' % (OUT, tag),
         'tapl Digit Rebuses', 'expect Puzzle 1/18', 'svg %s/%s-digit-start.svg' % (OUT, tag)]
    order = list(range(1, len(data))) + [0]
    for n, i in enumerate(order):
        p = data[i]
        for r in range(4):
            for c in range(4):
                L.append('tapl %d' % p['answer'][r][c])
        L.append('expect Solved!')
        if i == 4: L.append('svg %s/%s-digit-solved.svg' % (OUT, tag))
        if n < len(order) - 1: L.append('tapl >')
    # mistakes on the Example: the lowest admitted digit everywhere breaks a rule; Check blackens it
    p = data[0]
    low = [[min(SETS[p['glyphs'][r][c]]) for c in range(4)] for r in range(4)]
    assert broken(p, low)
    L += ['tapl Reset', 'expect Tap Reset again', 'tapl Reset']
    for r in range(4):
        for c in range(4):
            L.append('tapl %d' % low[r][c])
    L += ['expect Tap Check', 'tapl Check', 'expect broken', 'svg %s/%s-digit-errors.svg' % (OUT, tag),
          'tapl Reset', 'tapl Reset', 'tapl Check', 'expect No mistakes',
          'tapl List', 'expect Digit Rebuses - choose a puzzle', 'svg %s/%s-digit-list.svg' % (OUT, tag), 'tapl Back',
          'tapl ?', 'expect How to play', 'svg %s/%s-digit-help.svg' % (OUT, tag), 'key 27', 'expect Example',
          # Home key and the list's Home button lead to the shell; counts there
          'key 23', 'expect 18 / 18 solved', 'expect 0 / 15 solved', 'tapl Digit Rebuses', 'tapl List', 'tapl Home', 'expect 18 / 18 solved',
          'tapl Random', 'expect Puzzle ', 'key 26', 'expect 18 / 18 solved', 'key 27', 'expect_closed']
    return '\n'.join(L) + '\n'

for W, H, tag in ((600, 800, 'portrait'), (800, 600, 'landscape')):
    open('%s/digit-%s.script' % (T, tag), 'w').write(script(W, H, tag))
PY
for tag in portrait landscape; do
  rm -f "$T/rebus.sav" "$T/digit.sav" "$T/cfg" "$T/global.cfg"
  if REBUS_SCRIPT="$T/digit-$tag.script" "$T/ui3"; then echo "PASS digit rebuses $tag"; else echo "FAIL digit rebuses $tag"; RC=1; fi
done
grep -c '^1 ' "$T/digit.sav" | sed 's/^/digit puzzles saved solved: /'
# Digit Rebuses in Russian and German (cells, pad, messages, list, help)
for lg in ru de; do
  rm -f "$T/digit.sav" "$T/rebus.sav"; printf 'lang=%s\n' $lg > "$T/cfg"
  if [ $lg = ru ]; then TILE="Цифровые ребусы"; T1="Ребус 1/18"; CH="Проверить"; OKM="Ошибок нет"; LS="Выберите ребус"; HP="Как играть"; HM="Домой"; SOL="Решено!"; LST="Список"; else TILE="Ziffernrebusse"; T1="Rätsel 1/18"; CH="Prüfen"; OKM="Keine Fehler"; LS="Rätsel wählen"; HP="Spielregeln"; HM="Start"; SOL="Gelöst!"; LST="Liste"; fi
  python3 - "$T" "$lg" "$TILE" "$T1" "$CH" "$OKM" "$LS" "$HP" "$HM" "$SOL" "$LST" <<'PY'
import json, sys
T, lg, TILE, T1, CH, OKM, LS, HP, HM, SOL, LST = sys.argv[1:]
p = json.load(open('native/puzzlebook/digit/module02.json'))['puzzles'][1]
L = ['size 600 800', 'show', 'tapl ' + TILE, 'expect ' + T1, 'tapl ' + CH, 'expect ' + OKM, 'tapl ?', 'expect ' + HP, 'tap 300 400',
     'tapl ' + LST, 'expect ' + LS, 'tapl ' + HM, 'expect ' + TILE, 'tapl ' + TILE]
for r in range(4):
    for c in range(4): L.append('tapl %d' % p['answer'][r][c])
L += ['expect ' + SOL, 'quit']
open('%s/digit-%s.script' % (T, lg), 'w').write('\n'.join(L) + '\n')
PY
  if REBUS_SCRIPT="$T/digit-$lg.script" "$T/ui3"; then echo "PASS digit rebuses $lg"; else echo "FAIL digit rebuses $lg"; RC=1; fi
done
rm -f "$T/cfg" "$T/digit.sav"

# ---- Settings > Transfer progress: save to a file, load it into a clean state, merge rules, bad files, ru/de
P="$T/progress.json"
rm -f "$T/rebus.sav" "$T/digit.sav" "$T/cfg" "$T/global.cfg" "$P"
python3 - "$T" "$OUT" <<'PY'
import json, sys
T, OUT = sys.argv[1:3]
dg = json.load(open('native/puzzlebook/digit/module02.json'))['puzzles'][1]
L = ['size 600 800', 'show', 'tapl Digit Rebuses']
for r in range(4):
    for c in range(4): L.append('tapl %d' % dg['answer'][r][c])
L += ['expect Solved!', 'tapl Home', 'tapl Square Rebuses', 'tapl 1', 'tapl Home', 'tapl Settings',
      'expect Transfer progress', 'svg %s/portrait-settings-sync.svg' % OUT, 'tapl Save progress to a file', 'expect Progress saved.',
      'svg %s/portrait-settings-saved.svg' % OUT, 'quit']
open(T + '/prog-export.script', 'w').write('\n'.join(L) + '\n')
open(T + '/prog-import.script', 'w').write('\n'.join([
  'size 600 800', 'show', 'expect 0 / 18 solved', 'tapl Settings', 'tapl Load progress from a file', 'expect Load progress?', 'expect File created:',
  'svg %s/portrait-confirm.svg' % OUT, 'tapl Cancel', 'expect Transfer progress',
  'tapl Load progress from a file', 'tapl Load', 'expect Puzzles added: 2, updated: 0.',
  'tapl Load progress from a file', 'tapl Load', 'expect No new results found.', 'tapl Back', 'expect 1 / 18 solved', 'quit']) + '\n')
PY
if REBUS_SCRIPT="$T/prog-export.script" "$T/ui3"; then echo "PASS progress export"; else echo "FAIL progress export"; RC=1; fi
python3 - "$P" "$T/rebus.sav" <<'PY' || { echo "FAIL progress file content"; RC=1; }
import json, sys
b = json.load(open(sys.argv[1]))
assert b['bundleVersion'] == 1 and len(b['exportedAt']) >= 19
d = b['namespaces']['module_digit_rebus']['puzzles']
r = b['namespaces']['module_rebus']['puzzles']
assert list(d) == ['p01'] and d['p01']['solved'] is True and len(d['p01']['cells']) == 16 and d['p01']['cells']['0:0'] == 4
assert list(r) == ['p01'] and r['p01']['solved'] is False and len(r['p01']['cells']) == 1
assert r['p01']['updatedAt'] > 1700000000000 and d['p01']['updatedAt'] > 1700000000000
assert r['p01']['elapsedMs'] == 0 and r['p01']['checks'] == 0
print('PASS progress file content')
PY
rm -f "$T/rebus.sav" "$T/digit.sav"
if REBUS_SCRIPT="$T/prog-import.script" "$T/ui3"; then echo "PASS progress import into a clean state"; else echo "FAIL progress import"; RC=1; fi
python3 - "$P" "$T/rebus.sav" <<'PY' || { echo "FAIL imported stamp kept"; RC=1; }
import json, sys
b = json.load(open(sys.argv[1]))
want = b['namespaces']['module_rebus']['puzzles']['p01']['updatedAt']
line = open(sys.argv[2]).read().split('\n')[2]  # p01 is the second puzzle line (after the header and the sample)
assert line.split()[-1] == str(want), (line, want)
print('PASS imported stamp kept')
PY

# merge rules with a bundle written like the app's (extra fields, unknown module, unknown puzzle, floats, escapes)
python3 - "$T" <<'PY'
import json, sys
T = sys.argv[1]
rb = {p['id']: p for p in json.load(open('native/puzzlebook/rebus/module01.json'))['puzzles']}
dg = {p['id']: p for p in json.load(open('native/puzzlebook/digit/module02.json'))['puzzles']}
def rebus_cells(p):
    cells = {}
    given = {(g[0], g[1], g[2]) for g in p['givens']}
    def num(r, s):
        return p['total'] if (r, s) == (4, 4) else p['rows'][s]['result'] if r == 4 else (p['rows'][r]['nums'][s] if s < 4 else p['rows'][r]['result'])
    for r in range(5):
        for s in range(5):
            for k, ch in enumerate(num(r, s)):
                if (r, s, k) not in given: cells['%d:%d:%d' % (r, s, k)] = int(ch)
    return cells
def digit_cells(p):
    return {'%d:%d' % (r, c): p['answer'][r][c] for r in range(4) for c in range(4)}
def entry(cells, solved, upd=None, **extra):
    e = {'cells': cells, 'elapsedMs': 123456.0, 'checks': 3, 'solved': solved}
    if upd is not None: e['updatedAt'] = upd
    e.update(extra); return e
first_key = sorted(rebus_cells(rb['p01']))[0]
key4 = sorted(rebus_cells(rb['p04']))[0]   # an editable cell of p04
B1 = {'bundleVersion': 1, 'exportedAt': '2026-10-29T12:34:56.789',
  'namespaces': {
    'module_rebus': {'puzzles': {
        'p01': entry({first_key: 9}, False, 1000),              # older than the local one: local kept
        'p02': entry(rebus_cells(rb['p02']), True, 5, solvedAt='2026-10-01T10:00:00', firstSolveElapsedMs=61000),
        'p03': entry(rebus_cells(rb['p03']), True, 7),
        'p04': entry({key4: 3, 'zz': 4, '9:9:9': 1, '0:0:9': 2, '0:0:1': 12}, False, 20000000000000),
        'p99': entry({'0:0:0': 1}, True, 1)}, 'tutorial': {'completed': True, 'completedAt': '2026-10-01'}},
    'module_digit_rebus': {'puzzles': {
        'p01': entry({}, False, 99999999999999),               # nothing in it: ignored
        'p02': entry(digit_cells(dg['p02']), True, 9)}},
    'module_domino': {'puzzles': {'p01': {'placed': [[0, 1], [1.5e2, 'a"b\u00e9']], 'solved': False, 'nested': {'a': [1, [2, [3, {'b': None}]]]}}}},
    'settings': {'language': 'ru'}}}
json.dump(B1, open(T + '/bundle1.json', 'w'), indent=2)
B2 = {'bundleVersion': 1, 'namespaces': {
    'module_rebus': {'puzzles': {
        'p01': entry(rebus_cells(rb['p01']), True, 3),          # solved beats the local unsolved copy
        'p04': entry({key4: 5}, False, 30000000000000),     # later stamp than the local copy: wins
        'p02': entry({'0:0:0': 1}, False, 99999999999999)}}}}  # local copy is solved: kept
json.dump(B2, open(T + '/bundle2.json', 'w'))
open(T + '/prog-merge1.script', 'w').write('\n'.join(['size 600 800', 'show', 'tapl Settings', 'tapl Load progress from a file', 'expect File created: 2026-10-29 12:34',
  'tapl Load', 'expect Puzzles added: 4, updated: 0.', 'tapl Back', 'expect 2 / 15 solved', 'expect 2 / 18 solved', 'quit']) + '\n')
open(T + '/prog-merge2.script', 'w').write('\n'.join(['size 600 800', 'show', 'tapl Settings', 'tapl Load progress from a file', 'expect File created: ',
  'tapl Load', 'expect Puzzles added: 0, updated: 2.', 'tapl Back', 'expect 3 / 15 solved', 'expect 2 / 18 solved', 'quit']) + '\n')
PY
# local state for the merge: rebus p01 with one digit and a fresh stamp (older than nothing: 1000 < now), digit p01 solved
rm -f "$T/rebus.sav" "$T/digit.sav"
cp "$P" "$T/export.json"
python3 - "$T" <<'PY'
import sys, json
T = sys.argv[1]
open(T + '/prog-seed.script', 'w').write('\n'.join(['size 600 800', 'show', 'tapl Settings', 'tapl Load progress from a file', 'tapl Load', 'quit']) + '\n')
PY
REBUS_SCRIPT="$T/prog-seed.script" "$T/ui3" >/dev/null   # loads the export from above: rebus p01 (1 digit), digit p01 (solved)
cp "$T/bundle1.json" "$P"
if REBUS_SCRIPT="$T/prog-merge1.script" "$T/ui3"; then echo "PASS progress merge: added, kept newer local, ignored unknown/empty"; else echo "FAIL progress merge 1"; RC=1; fi
cp "$T/bundle2.json" "$P"
if REBUS_SCRIPT="$T/prog-merge2.script" "$T/ui3"; then echo "PASS progress merge: solved beats unsolved, later stamp wins, solved kept"; else echo "FAIL progress merge 2"; RC=1; fi
KEY4=$(python3 -c "import json;p=[q for q in json.load(open('native/puzzlebook/rebus/module01.json'))['puzzles'] if q['id']=='p04'][0];g={(x[0],x[1],x[2]) for x in p['givens']};print(next('%d:%d:%d'%(r,s,k) for r in range(5) for s in range(5) for k in range(3) if (r,s,k) not in g))")
python3 - "$T/rebus.sav" "$KEY4" <<'PY' || { echo "FAIL merged save file"; RC=1; }
import sys
lines = open(sys.argv[1]).read().split('\n')
KEY4 = sys.argv[2]
p01, p04 = lines[2].split(), lines[5].split()
assert p01[0] == '1' and p01[2] == '3'          # solved, stamp of the incoming copy
assert p04[0] == '0' and p04[2] == '30000000000000'  # unsolved, stamp of the winner
r_, s_, k_ = map(int, KEY4.split(':'))
assert p04[1][r_ * 15 + s_ * 3 + k_] == '5', p04[1]
print('PASS merged save file')
PY
# bad files and a missing file
for case in none garbage array nobundle newer; do
  case $case in
    none) rm -f "$P"; EXP="There is no puzzlebook-progress.json";;
    garbage) printf '{not json' > "$P"; EXP="That is not a progress file.";;
    array) printf '[1, 2, 3]' > "$P"; EXP="That is not a progress file.";;
    nobundle) printf '{"foo": 1}' > "$P"; EXP="That is not a progress file.";;
    newer) printf '{"bundleVersion": 2, "namespaces": {}}' > "$P"; EXP="made by a newer version";;
  esac
  printf 'size 600 800\nshow\ntapl Settings\ntapl Load progress from a file\nexpect %s\nexpect Transfer progress\nquit\n' "$EXP" > "$T/prog-bad.script"
  if REBUS_SCRIPT="$T/prog-bad.script" "$T/ui3"; then echo "PASS progress file $case"; else echo "FAIL progress file $case"; RC=1; fi
done
# Russian and German texts of the section, and the landscape layout
for lg in ru de; do
  printf 'lang=%s\n' $lg > "$T/cfg"
  if [ $lg = ru ]; then EXPS="Перенос прогресса"; EXPE="Прогресс сохранён."; BK="Назад"; else EXPS="Fortschritt übertragen"; EXPE="Fortschritt gespeichert."; BK="Zurück"; fi
  for sz in "600 800 portrait" "800 600 landscape"; do
    set -- $sz
    printf 'size %s %s\nshow\ntap 40 760\n' $1 $2 > "$T/prog-$lg.script"
    printf 'tapl %s\nexpect %s\nsvg %s/%s-settings-sync-%s.svg\nquit\n' "$( [ $lg = ru ] && echo Настройки || echo Einstellungen )" "$EXPS" "$OUT" $3 $lg >> "$T/prog-$lg.script"
    sed -i '/^tap 40 760$/d' "$T/prog-$lg.script"
    if REBUS_SCRIPT="$T/prog-$lg.script" "$T/ui3"; then echo "PASS settings transfer section $lg $3"; else echo "FAIL settings transfer section $lg $3"; RC=1; fi
  done
done
rm -f "$T/cfg" "$P" "$T/export.json" "$T/rebus.sav" "$T/digit.sav"

# ---- languages and the Settings screen: Russian and German walk-throughs, "system" follows the reader's global.cfg,
# the choice is saved to the cfg file and read back by the next start
for tag in portrait landscape; do
  if [ $tag = portrait ]; then SZ="600 800"; else SZ="800 600"; fi
  rm -f "$T/cfg" "$T/global.cfg" "$T/rebus.sav"
  cat > "$T/lang-$tag.script" <<EOS
size $SZ
show
expect Puzzle Book
expect 0 / 15 solved
tapl Settings
expect Language
expect System default
svg $OUT/$tag-settings-en.svg
tapl Русский
expect Язык
expect Как в системе
svg $OUT/$tag-settings-ru.svg
tapl Назад
expect Головоломки
expect Случайный
expect решено 0 / 15
expect Ребусы с квадратиками
svg $OUT/$tag-home-ru.svg
tapl ?
expect Головоломки - справка
svg $OUT/$tag-homehelp-ru.svg
tap 300 400
tapl Ребусы с квадратиками
expect Ребус 1/15
expect Проверить
expect Стереть
svg $OUT/$tag-play-ru.svg
tapl Проверить
expect Ошибок нет
tapl Сбросить
expect «Сбросить» ещё раз
tapl Список
expect Выберите ребус
svg $OUT/$tag-list-ru.svg
tapl Назад
tapl ?
expect Как играть
svg $OUT/$tag-help-ru.svg
tap 300 400
tapl Домой
tapl Настройки
tapl Deutsch
expect Sprache
expect Systemsprache
tapl Zurück
expect Rätselbuch
expect Zufall
expect gelöst 0 / 15
expect Zahlenrätsel-Quadrate
svg $OUT/$tag-home-de.svg
tapl Zahlenrätsel-Quadrate
expect Rätsel 1/15
expect Prüfen
tapl Prüfen
expect Keine Fehler
svg $OUT/$tag-play-de.svg
tapl ?
expect Spielregeln
svg $OUT/$tag-help-de.svg
tap 300 400
tapl Reset
expect Noch einmal
tapl Liste
expect Rätsel wählen
tapl Zurück
key 27
expect Rätselbuch
key 23
expect Einstellungen
key 27
expect Rätselbuch
tapl Einstellungen
tapl English
expect Language
tapl Back
expect Puzzle Book
quit
EOS
  if REBUS_SCRIPT="$T/lang-$tag.script" "$T/ui"; then echo "PASS languages $tag"; else echo "FAIL languages $tag"; RC=1; fi
  if [ "$(cat "$T/cfg")" = "lang=en" ]; then echo "PASS language saved"; else echo "FAIL language saved: $(cat "$T/cfg")"; RC=1; fi
done

# the list screen's Home button in Russian and German
rm -f "$T/cfg" "$T/rebus.sav"
printf 'lang=ru\\n' > "$T/cfg"
printf 'size 600 800\\nshow\\ntapl Ребусы с квадратиками\\ntapl Список\\nexpect Выберите ребус\\ntapl Домой\\nexpect Головоломки\\nexpect решено\\nquit\\n' > "$T/list-ru.script"
if REBUS_SCRIPT="$T/list-ru.script" "$T/ui"; then echo "PASS list Home ru"; else echo "FAIL list Home ru"; RC=1; fi
printf 'lang=de\\n' > "$T/cfg"
printf 'size 800 600\\nshow\\ntapl Zahlenrätsel-Quadrate\\ntapl Liste\\nexpect Rätsel wählen\\ntapl Start\\nexpect Rätselbuch\\nexpect gelöst\\nquit\\n' > "$T/list-de.script"
if REBUS_SCRIPT="$T/list-de.script" "$T/ui"; then echo "PASS list Home de"; else echo "FAIL list Home de"; RC=1; fi

# system default follows the reader's language (language=ru in global.cfg); an unknown one falls back to English
rm -f "$T/cfg" "$T/rebus.sav"
printf 'timezone=Europe/Moscow\nlanguage=ru\n' > "$T/global.cfg"
printf 'size 600 800\nshow\nexpect Головоломки\ntapl Настройки\nexpect Как в системе\ntapl Deutsch\nexpect Einstellungen\nquit\n' > "$T/sys-ru.script"
if REBUS_SCRIPT="$T/sys-ru.script" "$T/ui"; then echo "PASS system language ru"; else echo "FAIL system language ru"; RC=1; fi
# next start: the saved fixed language wins over the reader's one
printf 'size 600 800\nshow\nexpect Rätselbuch\nexpect Zufall\nquit\n' > "$T/sys-de.script"
if REBUS_SCRIPT="$T/sys-de.script" "$T/ui"; then echo "PASS saved language wins"; else echo "FAIL saved language wins"; RC=1; fi
printf 'language=fr\n' > "$T/global.cfg"; rm -f "$T/cfg"
printf 'size 600 800\nshow\nexpect Puzzle Book\nquit\n' > "$T/sys-fr.script"
if REBUS_SCRIPT="$T/sys-fr.script" "$T/ui"; then echo "PASS unknown system language -> English"; else echo "FAIL unknown system language"; RC=1; fi
rm -f "$T/cfg" "$T/global.cfg"

if [ -n "$KEEP" ] || [ "$OUT" != "$T" ]; then
  if node -e "require('playwright')" 2>/dev/null; then
    OUT="$OUT" node -e "
      const {chromium}=require('playwright');const fs=require('fs');
      (async()=>{const b=await chromium.launch();const p=await b.newPage();
        for(const f of fs.readdirSync(process.env.OUT).filter(x=>x.endsWith('.svg'))){
          const svg=fs.readFileSync(process.env.OUT+'/'+f,'utf8');const m=svg.match(/width='(\d+)' height='(\d+)'/);
          await p.setViewportSize({width:+m[1],height:+m[2]});
          await p.setContent('<body style=\"margin:0\">'+svg+'</body>');
          await p.screenshot({path:process.env.OUT+'/'+f.replace('.svg','.png')});}
        await b.close();})();"
  fi
fi
[ "$OUT" = "$T" ] && rm -rf "$T"
exit $RC
