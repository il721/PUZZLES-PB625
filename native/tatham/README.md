# Puzzle Collection (native/tatham)

Simon Tatham's Portable Puzzle Collection (40 logic puzzles, MIT licence) as a native app for the PocketBook 625 (600x800; the same source also runs on the Verse Pro PB634, 1072x1448, see il721/PB634):
touch UI, e-ink friendly (no animations, partial updates, a full refresh every few steps), English / Russian / German.

## What it does
- **Home:** the 40 puzzles in a 2-column list (2 pages); a note shows which are in progress or solved. Menu: language, thick lines, clean the screen, exit.
- **Play:** board fitted to the screen, status line (moves, mines left, ...), Undo / Redo, **Left button / Right button** switch for taps, a digit keypad for puzzles that need one (Sudoku, KenKen, Towers, ...), Menu.
- **Touch:** tap = left button, hold a finger = right button, drag = drag. The switch at the bottom makes plain taps right-button taps (Pattern, Loopy, Sudoku pencil marks use both buttons).
- **Menu in a puzzle:** new game, restart, show solution, size and difficulty (every preset of the puzzle), **custom settings** (width, height, difficulty, ... with the on-screen keyboard), rules and controls (shown in the largest font that fits, up to 80 px), language, thick lines, clean screen, back to the list, exit.
- **Export / import (home menu):** *Export all saves (zip)* writes `/mnt/ext1/tatham-saves.zip` with `<puzzle>.sav` and `<puzzle>.st` of every puzzle that has a save; *Import saves (zip)* restores them from the same path (saves of those puzzles are replaced). Only uncompressed ("stored") entries are read; the PB634 app (il721/PB634) and the Android port (il721/meizu-m6-note-m721-h) write the same layout, so progress moves between devices by copying the file. `savezip.c`; host test: zip checked with Python's zipfile, wipe-and-import round trip byte for byte. Not tested on the reader: the `Message()` box.
- **Saves:** every move is saved to `/mnt/ext1/tatham/<puzzle>.sav` (the engine's own save format, includes undo history and size/difficulty); opening a puzzle resumes it. Settings (language, thick lines) are in `/mnt/ext1/tatham/tatham.cfg`; fatal engine errors are appended to `/mnt/ext1/tatham/tatham.log`.
- **Hardware keys:** page keys = Undo / Redo (list pages on the home screen), Menu key = menu, Back = back (home, then exit).
- **Text size:** buttons, lists and the status line are sized for the 1072 px wide Verse Pro (40 px, titles 50 px, notes 30 px, rules up to 80 px) and scaled by screen width, so about 22 / 28 / 17 / 45 px on the 600 px wide 625 (same physical size); they shrink one step at a time when a label is long (e.g. Russian or German).
- **Languages:** Settings come from the menu; "System default" reads `language=` from `/mnt/ext1/system/config/global.cfg`. Puzzle names, rules, presets, option labels and status texts are translated (`tr.c`, `rules.c`); text the glossary does not know stays English.

## Install and build
- Copy `reader/Puzzle Collection.app` to `<drive>:\applications`, eject, open "Puzzle Collection" from the Applications menu. Saves go to `/mnt/ext1/tatham/`.
- Build (PocketBook SDK 4.8.1, see the repo README "Build"): `make -C native/tatham -j8 SDK=$PWD/sdk` writes `reader/Puzzle Collection.app` (about 1 MB). Needs `libinkview`, `libc` and `libm` (all symbols GLIBC_2.4; `clock_gettime` is avoided because it would pull in `librt`). Check `readelf -V "reader/Puzzle Collection.app"`: only GLIBC_2.4. Two details keep it there: `-D_GNU_SOURCE` (otherwise the glibc 2.9 headers turn `sscanf` into `__isoc99_sscanf`, GLIBC_2.7) and `tat.h` maps the key names (`KEY_BACK`, the old SDK's names) to the `IV_KEY_*` used by the code.
- The same sources build for the PB634 with its newer SDK (il721/PB634, `apps/tatham`, CMake).

## Source layout
- `puzzles/` upstream source of the engine and the 40 puzzles, unmodified (snapshot 20260923.616da16, see `puzzles/VENDORED.txt`), built as one binary with `-DCOMBINED`. Printing and other platforms' front ends are not included.
- `fe.c` the engine's drawing API on InkView: rectangles and lines with `FillArea` / `DrawLine`, polygons through the engine's own rasteriser, circles as scan lines, text with `DrawTextRect`, blitters with `BitmapFromScreen` / `DrawBitmap`; colours are mapped to the panel's 16 grays (anything light becomes pure white). Animations are skipped: the engine's timers are run to the end at once.
- `main.c` the UI and the touch handling; `tr.c` / `rules.c` the translations; `tat.h` shared declarations.
- `test/` host tests (the fake InkView draws real Liberation glyphs with FreeType when it is installed, otherwise bars).

## Tests (cloud sandbox, no reader)
`sh test/test.sh` builds the real `main.c`, `fe.c`, `tr.c`, `rules.c` and all 40 puzzles for the host with ASan + UBSan against `test/fake_inkview.c` (an 8-bit gray framebuffer with clipping and bitmaps, driven by a script) and checks:
- every puzzle in turn: open, 25 random taps / holds / drags, rules page, menu, show solution, back to the list; 40 save files exist afterwards and the board is drawn;
- restart: saved puzzles are listed as in progress and resume;
- size and difficulty lists, custom settings with the keyboard, the "new puzzle?" dialog, thick lines, hardware keys;
- Russian and German text on the home list, in a puzzle, in menus, presets, option labels; the language setting is saved; "System default" follows `global.cfg`;
- a 600x800 screen and rotation;
- the no-text-cut-off check below runs at 1072x1448 and at 600x800;
- all 40 puzzles have a Russian and German name and rules;
- no text is cut off on the home list, play screen, menus, presets, custom settings, rules and the solved state, in all three languages (measured with the real Liberation fonts).

The only exemption: UBSan's `shift-base` check is off for the vendored files, because upstream `draw-poly.c` shifts negative numbers (harmless).

Found by the tests and fixed: an upstream out-of-bounds read in Train Tracks when a drag starts outside the grid (the app now drops the rest of a gesture whose press the engine did not use); the engine only repeats a status-bar text when it changes, so the app keeps the text across repaints.

## Not tested on the reader
Everything above ran on the host. Not checked on the real reader (PB625): how the drawing calls behave with the firmware's `SetClip` and `DrawTextRect`, that `BitmapFromScreen` works for the dragged pieces (Galaxies, Map, Pegs, Signpost, Guess, Inertia), `EVT_POINTERLONG` timing for the right button, text sizes and positions with the real fonts (the host tests use the real Liberation metrics, not the firmware's renderer), Cyrillic rendering, refresh speed and ghosting (partial updates, full refresh every 30 board changes and every 5th screen change), and generation times of the biggest presets (on this machine the slowest takes 0.3 s; the reader is much slower, a "Generating..." box is shown).
If something looks wrong paste what the screen showed and `/mnt/ext1/tatham/tatham.log` if it exists.

## Not done
Game IDs / seeds and game description entry, the engines' own preferences (keyboard shortcuts, highlight options), printing, multi-line help beyond the short rules, landscape tuning (it works, the board is just smaller).
