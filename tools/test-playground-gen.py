#!/usr/bin/env python3
"""Script generator for the Puzzle games (Playground) part of tools/test-rebus-ui.sh.

usage: test-playground-gen.py T OUT tag W H geom|play

geom  writes T/pg-geom-<tag>.script: opens the module and every game once. The test build (-DPG_GEOM_LOG) logs where each
      node, slot, tray tile and board cell is drawn; the 'play' phase reads that log (T/log.txt).
play  writes the scripts that tap through the recorded solutions of the source (module06.json) of all seven games, the
      undo / reset / list / help behaviour, a restart, the Russian and German screens and the progress transfer."""
import json, re, sys

T, OUT, tag, W, H, phase = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
N = 'native/puzzlebook/playground/'
IDS = ['eight_chips', 'cats_dogs', 'hourglass', 'three_each', 'patterns5', 'patterns4', 'swap_blocks']
TITLES = ['Eight chips', 'Cats and dogs', 'Hourglass', 'Three of each', 'Patterns 5×5', 'Patterns 4×4', 'Swap the squares']
PAR = [28, 32, 26, 0, 0, 0, 32]

def put(name, lines): open('%s/%s' % (T, name), 'w').write('\n'.join(lines) + '\n')

if phase == 'geom':
    put('pg-geom-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games'] + ['tapl >'] * 6 + ['quit'])
    sys.exit(0)

# where everything is: the last block the module logged for each game
geo, cur = {}, None
for line in open(T + '/log.txt'):
    p = line.split()
    if not p or not p[0].startswith('pg'): continue
    k, a, b, c = p[0], int(p[1]), int(p[2]), int(p[3])
    if k == 'pgg': cur = a; geo[cur] = {'n': {}, 's': {}, 't': {}, 'c': None}
    elif k == 'pgn': geo[cur]['n'][a] = (b, c)
    elif k == 'pgs': geo[cur]['s'][a] = (b, c)
    elif k == 'pgt': geo[cur]['t'][a] = (b, c)
    elif k == 'pgc': geo[cur]['c'] = (a, b, c)
assert sorted(geo) == list(range(7)), sorted(geo)

src = open(N + 'pg_core.c').read()
def names(var): return re.search(var + r'\[\] = *\n? *"([^"]*)"', src).group(1).split()
NODES = {0: names('EIGHT_NODES'), 1: names('CATS_NODES'), 2: names('HOUR_NODES')}
games = {g['id']: g for g in json.load(open(N + 'module06.json'))['games']}

def tap(xy): return 'tap %d %d' % xy

def moves(g, upto=None):
    """taps that play the first `upto` moves of the recorded solution of game g"""
    L = []
    for mv in games[IDS[g]]['solution'][:upto]:
        if g <= 2:                                   # a piece, then where it goes
            n, nm = geo[g]['n'], NODES[g]
            L += [tap(n[nm.index(mv[0])]), tap(n[nm.index(mv[-1])])]
        elif g <= 5:                                 # a tray tile (a tap more per quarter turn), then the slot
            t = int(mv[0][1:]) - 1
            slot, rot = re.match(r'S(\d+)r(\d)', mv[1]).groups()
            L += [tap(geo[g]['t'][t])] * (1 + int(rot)) + [tap(geo[g]['s'][int(slot) - 1])]
        else:                                        # a block (a tap on its first cell), then the dot twice: preview, move
            sx, sy, cs = geo[g]['c']
            def cell(cid):
                r, c = re.match(r'r(\d+)c(\d+)', cid).groups()
                return (sx + (int(c) - 1) * cs + cs // 2, sy + (int(r) - 1) * cs + cs // 2)
            L += [tap(cell(mv[0])), tap(cell(mv[1])), tap(cell(mv[1]))]
    return L

# --- every game solved by taps
L = ['size %d %d' % (W, H), 'show', 'expect 0 / 7 solved', 'tapl Puzzle games']
for g in range(7):
    L += ['expect ' + TITLES[g]]
    if g == 0: L += ['svg %s/%s-pg-start.svg' % (OUT, tag)]
    L += moves(g) + ['expect Solved']
    if PAR[g]: L += ['expect Solved in %d moves' % PAR[g], 'expect That is the minimum']
    L += ['svg %s/%s-pg-solved%d.svg' % (OUT, tag, g), 'tapl >']
L += ['expect Eight chips  1/7  ok', 'tapl List', 'expect choose one', 'svg %s/%s-pg-list.svg' % (OUT, tag), 'tapl Back', 'tapl Home', 'expect 7 / 7 solved', 'quit']
put('pg-play-%s.script' % tag, L)

# --- cats and dogs played halfway, resumed after a restart, then two undos
put('pg-half-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games', 'tapl >'] + moves(1, 10) + ['expect Moves: 10', 'quit'])
put('pg-resume-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games', 'expect Cats and dogs', 'expect Moves: 10', 'tapl Undo', 'expect Moves: 9',
                                  'tapl Undo', 'expect Moves: 8', 'svg %s/%s-pg-cats8.svg' % (OUT, tag), 'quit'])

# --- eight chips: selecting, letting go, an illegal tap, undo, reset (armed first), the rules page
n0, nm0 = geo[0]['n'], NODES[0]
def node(name): return tap(n0[nm0.index(name)])
put('pg-undo-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games', 'expect Moves: 0', 'expect Tap a piece',
    node('P2'), 'expect This piece',                                    # P2 is boxed in
    node('C'), 'expect Tap a piece', 'expect_not Tap a marked',        # a free place: nothing happens
    node('P1'), 'expect Tap a marked', 'svg %s/%s-pg-selected.svg' % (OUT, tag),
    node('P1'), 'expect_not Tap a marked',                              # the same piece again lets go
    node('P1'), node('P3'), 'expect Tap a marked',                      # another piece is no destination: the selection moves to it
    node('C'), 'expect Moves: 1',
    node('P6'), node('P3'), 'expect Moves: 2',
    'tapl Undo', 'expect Moves: 1', 'tapl Undo', 'expect Moves: 0', 'tapl Undo', 'expect Moves: 0',
    node('P1'), node('C'), 'expect Moves: 1',
    'tapl Reset', 'expect Tap Reset', 'tapl Undo', 'expect_not Tap Reset', 'expect Moves: 0',
    node('P1'), node('C'), 'tapl Reset', 'tapl Reset', 'expect Moves: 0',
    'tapl ?', 'expect Pieces 1-8', 'svg %s/%s-pg-help.svg' % (OUT, tag), 'tap 300 400', 'expect Eight chips', 'quit'])

# --- the tile games and swap blocks: selecting, turning, taking back, the dot preview
def tile_tap(g, t): return tap(geo[g]['t'][t])
def slot_tap(g, s): return tap(geo[g]['s'][s])
put('pg-tiles-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games', 'tapl >', 'tapl >', 'tapl >', 'expect Three of each', 'expect Tiles laid: 0 of 9',
    'tapl Rotate', 'expect Select a tile first',
    tile_tap(3, 0), 'tapl Rotate', 'tapl Rotate', slot_tap(3, 4), 'expect Tiles laid: 1 of 9', 'expect Moves: 1',   # T1 turned twice, laid in the middle
    slot_tap(3, 4), 'expect Rotate turns', 'tapl Rotate', 'expect Moves: 2',                                          # select the laid tile, turn it (a move)
    slot_tap(3, 4), 'expect Moves: 3',                                                                                # a second tap turns it again
    'tapl To tray', 'expect Tiles laid: 0 of 9', 'expect Moves: 4',
    'tapl To tray', 'expect Select a tile first',
    tile_tap(3, 2), tile_tap(3, 2), slot_tap(3, 0), tile_tap(3, 5), slot_tap(3, 0), 'expect Tiles laid: 1 of 9',      # slot 0 is taken: the tile stays in the tray
    'svg %s/%s-pg-tiles.svg' % (OUT, tag),
    'tapl Undo', 'expect Moves: 4', 'tapl Undo', 'expect Moves: 3', 'quit'])
def swap_cell(r, c): sx, sy, cs = geo[6]['c']; return tap((sx + c * cs + cs // 2, sy + r * cs + cs // 2))
put('pg-swap-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games'] + ['tapl >'] * 6 + ['expect Swap the squares', 'expect Tap a block',
    swap_cell(0, 0), 'expect This block cannot',                       # the top-left block is boxed in
    swap_cell(3, 0), 'expect Tap a dot to preview',                   # the left block of the third band can slide down into the free rows
    swap_cell(7, 0), 'expect Tap the dot again', 'expect Moves: 0', 'svg %s/%s-pg-preview.svg' % (OUT, tag),
    swap_cell(7, 0), 'expect Moves: 1', 'expect Tap a block',
    swap_cell(7, 0), 'expect Tap a dot to preview',                   # the moved block again (its anchor cell is covered by it)
    swap_cell(9, 7), 'expect_not Tap a dot', 'expect Tap a block',    # a free place that is no dot: the selection is dropped
    'tapl Undo', 'expect Moves: 0', 'quit'])

# --- Russian and German: the first moves, a tile game, the solved message
LANG = {'ru': dict(tile='Головоломки-игры', title='Восемь фишек', hint='Нажмите фишку', moves='Ходов: 1', solved='Решено за 28 ходов!', opt='Это минимум!',
                   tiles='Выложено: 1 из 9', g3='Всюду по три', rot='Повернуть', tray='В набор', lst='Выберите игру', st='Список', rules='Фишки 1–8',
                   undo='Отмена', done='решена', best='Рекорд', pick='Сначала выберите'),
        'de': dict(tile='Denkspiele', title='Acht Chips', hint='Stein antippen', moves='Züge: 1', solved='Gelöst in 28 Zügen!', opt='Das ist das Minimum!',
                   tiles='Gelegt: 1 von 9', g3='Überall drei', rot='Drehen', tray='In Vorrat', lst='Spiel wählen', st='Liste', rules='Die Chips 1–8',
                   undo='Rückgängig', done='gelöst', best='Bestwert', pick='Wählen Sie zuerst')}
for lg, d in LANG.items():
    L = ['size %d %d' % (W, H), 'show', 'tapl ' + d['tile'], 'expect ' + d['title'], 'expect ' + d['hint']] + moves(0, 1) + ['expect ' + d['moves'], 'tapl ' + d['undo'],
         'tapl ?', 'expect ' + d['rules'], 'tap 300 400'] + moves(0) + ['expect ' + d['solved'], 'expect ' + d['opt'], 'expect ' + d['best'],
         'tapl >', 'tapl >', 'tapl >', 'expect ' + d['g3'], 'tapl ' + d['rot'], 'expect ' + d['pick']] + [tile_tap(3, 0), slot_tap(3, 0), 'expect ' + d['tiles'], 'svg %s/%s-pg-%s-tiles.svg' % (OUT, tag, lg),
         'tapl ' + d['st'], 'expect ' + d['lst'], 'expect ' + d['done'], 'svg %s/%s-pg-%s.svg' % (OUT, tag, lg), 'quit']
    put('pg-%s-%s.script' % (lg, tag), L)

# --- progress transfer: export (eight chips solved, cats and dogs half played), then app-shaped bundles
put('pg-export-%s.script' % tag, ['size %d %d' % (W, H), 'show', 'tapl Puzzle games'] + moves(0) + ['expect Solved', 'tapl >'] + moves(1, 10) + ['expect Moves: 10',
    'tapl Home', 'tapl Settings', 'tapl Save progress to a file', 'expect Progress saved.', 'quit'])
INIT = {'eight_chips': {'positions': {**{'P%d' % i: str(i) for i in range(1, 9)}, 'C': None}},
        'three_each': {'slots': [None] * 9}, 'patterns5': {'slots': [None] * 25}, 'patterns4': {'slots': [None] * 16},
        'hourglass': {'positions': {k: ('T' if i < 15 else None) for i, k in enumerate(NODES[2])}},
        'cats_dogs': {'positions': {k: ('C' if k[0] == 'L' and k[1] in '123' else 'D' if k[0] == 'R' and k[1] in '123' else None) for k in NODES[1]}},
        'swap_blocks': {'origins': [0, 4, 10, 18, 24, 28, 80, 84, 90, 98, 104, 108]}}
def entry(g, solved, upd, best=None, **extra):
    e = dict(INIT[g]); e.update({'moveStack': [], 'moveCount': 0, 'solved': solved, 'elapsedMs': 5000.0, 'updatedAt': upd}); e.update(extra)
    if best is not None: e['bestMoves'] = best
    return e
B1 = {'bundleVersion': 1, 'exportedAt': '2026-10-29T12:34:56.789', 'namespaces': {'module_playground': {'puzzles': {
    'eight_chips': entry('eight_chips', True, 1700000000000, 30, solvedAt='2026-10-01T10:00:00', firstSolveElapsedMs=4000),
    'cats_dogs': dict(entry('cats_dogs', False, 1800000000000), moveStack=[['L1', 'U1']], moveCount=1),   # in progress: its moves cannot travel, ignored
    'hourglass': entry('hourglass', True, 1700000000001, 20),                                            # best below the proven minimum 26: ignored
    'three_each': entry('three_each', False, 1800000000000),
    'patterns5': entry('patterns5', True, 1700000000002),
    'swap_blocks': entry('swap_blocks', True, 1700000000003),
    'p99': {'solved': True, 'updatedAt': 1}}}}}
B2 = {'bundleVersion': 1, 'namespaces': {'module_playground': {'puzzles': {
    'eight_chips': entry('eight_chips', True, 1700000000004, 28),    # fewer moves: wins
    'cats_dogs': entry('cats_dogs', True, 1700000000005, 32),         # solved, local copy untouched: added
    'hourglass': entry('hourglass', True, 1700000000006, 26),         # local has no best: wins
    'patterns5': entry('patterns5', True, 1700000000007),             # a tie: nothing
    'swap_blocks': entry('swap_blocks', True, 1700000000008, 40)}}}}
json.dump(B1, open(T + '/bundle-pg1.json', 'w'), indent=2)
json.dump(B2, open(T + '/bundle-pg2.json', 'w'))
imp = lambda n, a, u, home: ['size %d %d' % (W, H), 'show', 'tapl Settings', 'tapl Load progress from a file', 'tapl Load',
                             'expect ' + ('Puzzles added: %d, updated: %d.' % (a, u) if a >= 0 else 'No new results found.'), 'tapl Back', 'expect ' + home, 'quit']
put('pg-imp1-%s.script' % tag, imp(1, 4, 0, '4 / 7 solved'))
put('pg-imp2-%s.script' % tag, imp(2, 1, 3, '5 / 7 solved'))
put('pg-imp3-%s.script' % tag, imp(3, -1, 0, '5 / 7 solved'))
