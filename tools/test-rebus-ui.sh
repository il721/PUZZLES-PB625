#!/bin/sh
# Runs the Puzzle Book shell and the Square Rebuses module (native/puzzlebook) on the build host against tools/fake-inkview (SVG output) and plays
# every puzzle through the on-screen buttons, portrait and landscape. Prints PASS/FAIL.
# Optional: OUT=dir keeps the SVG screenshots (and PNGs if playwright is installed).
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
OUT=${OUT:-$T}
mkdir -p "$OUT"
N=native/puzzlebook
gcc -Wall -Wextra -std=gnu99 -Itools/fake-inkview -c tools/fake-inkview/inkview_svg.c -o "$T/shim.o"
for f in $N/shell.c $N/ui.c $N/modules.c $N/rebus/module_rebus.c $N/rebus/rebus_core.c; do
  gcc -Wall -Wextra -std=gnu99 -Dmain=rebus_main -DSAVEF="\"$T/rebus.sav\"" -DLOGF="\"$T/log.txt\"" \
    -Itools/fake-inkview -I$N -I$N/rebus -c $f -o "$T/$(basename $f .c).o"
done
gcc "$T"/*.o -o "$T/ui"

# second variant: two modules (template module registered through tools/test-modules2.c)
T2="$T/two"; mkdir -p "$T2"
for f in $N/shell.c $N/ui.c tools/test-modules2.c $N/rebus/module_rebus.c $N/rebus/rebus_core.c $N/example/module_example.c; do
  sv=rebus.sav; case $f in *example*) sv=example.sav;; esac
  gcc -Wall -Wextra -std=gnu99 -Dmain=rebus_main -DSAVEF="\"$T/$sv\"" -DLOGF="\"$T/log.txt\"" \
    -Itools/fake-inkview -I$N -I$N/rebus -c $f -o "$T2/$(basename $f .c).o"
done
gcc "$T/shim.o" "$T2"/*.o -o "$T/ui2"

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
          'tapl Home', 'expect 14 / 15 solved', 'tapl Random', 'expect Puzzle 15/15', 'tapl Home',
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
