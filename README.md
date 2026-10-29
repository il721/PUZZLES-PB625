# PUZZLES-PB625: puzzle games for the PocketBook 625

`reader/Puzzle Book.app` is a native touch app (ARM ELF, PocketBook inkview) with a home screen and puzzle **modules**. Modules, ported from https://github.com/il721/puzzles: **Square Rebuses** (`packages/module_rebus`: 15 puzzles + the "Example" sample; `native/puzzlebook/rebus/module01.json` is that repo's puzzle data) and **Digit Rebuses** (`packages/module_digit_rebus`: 18 puzzles + "Example"; `native/puzzlebook/digit/module02.json`). Moved here from il721/PB625 on 2026-10-29 (clean first commit); Wi-Fi, file transfer and the Total Commander plugin stay in il721/PB625.

## Device facts
- PocketBook 625 (Basic Touch 2), firmware R4.4.1585, Linux 3.0.8 armv7l, 6" e-ink touch. Screen 600x800 portrait / 800x600 landscape; the app lays out for both (`EVT_ORIENTATION`).
- Apps run as uid 102 `sreader`, no root. The internal storage `/mnt/ext1` (vfat, no exec bit) is the USB drive; while USB is connected it is unmounted on the reader. Apps live in `<drive>:\applications\*.app`.
- Native apps link against the firmware's `libinkview.so` and libc. The SDK 4.8.1 toolchain targets glibc 2.9; the binary here needs only GLIBC_2.4.
- Confirmed by the user on the reader (2026-10-29): the Puzzle Book build with home screen + Square Rebuses works. The build with Settings and the three languages was tested only in the sandbox. To confirm on the device: Cyrillic text renders (UTF-8 in `DrawString`, LiberationSans), and "System default" picks the reader's language (see the `language setting` line in `puzzlebook.txt`).

## What the app does
- Home: one tile per module (name, "solved / total", description), **Random** (random unsolved puzzle from any module, weighted by how many each module has left), `?` help, **Exit**, **Settings** (bottom left; Menu key too).
- Languages and Settings (ported from il721/puzzles, same set: Русский, English, Deutsch): Settings has one section, Language: System default / Русский / English / Deutsch. "System default" reads `language=xx` from the firmware's `/mnt/ext1/system/config/global.cfg` (unknown or missing -> English, like the source). The choice is saved in `/mnt/ext1/puzzlebook.cfg` (`lang=system|ru|en|de`). Settings also has **Transfer progress** (like the source): "Save progress to a file" writes `/mnt/ext1/puzzlebook-progress.json`, "Load progress from a file" reads the same path, shows the file's date and asks Cancel / Load, then merges. The file is the source app's bundle format (`bundleVersion` 1, namespaces `module_rebus` and `module_digit_rebus`, one entry per touched puzzle with `cells`, `solved`, `updatedAt`), so it can be exchanged with the Flutter app in both directions. Merge (`progress.c`, the source's `mergePuzzleEntry` as far as this app tracks it): solved beats unsolved; two solved copies tie; between two unsolved copies the later `updatedAt` wins (a stamp beats none; older saves without stamps lose to a stamped incoming copy); ties keep the local copy, so loading a file twice changes nothing. Unknown namespaces/puzzles/cells are ignored. Not tracked here: elapsed time, check count, first-solve time (written as 0). Not ported: sound (the reader has no speaker), dark theme (not sensible on e-ink, would need inkview invert calls not yet checked on the device).
- Opening a tile hands all events to the module until it calls `shell_leave()` (Home button or Back key).
- Digit Rebuses: a 4x4 square of digit cells, every row and column an equation `a op b op c = d` strictly left to right (`+ - x :`, `:` exact, no intermediate value below zero), no givens. Each cell is masked by one of five glyphs that admits a few digits ({0,8,9} {0,6,8} {1,4,7} {2,3} {3,5,9}). The source draws the glyph as a coloured border; on e-ink an empty cell shows the digits its glyph admits (small, grey) and the pad offers only those digits plus Del. Check blackens the cells of broken rules; a full but wrong grid says so. Win = any grid that satisfies all rules. Same buttons and keys as Square Rebuses (List, ?, Home, `<` `>`, Reset twice; Back/Home/Menu keys). Not ported: the timer, check counter and win dialog (neither module has them), the guided tutorial.
- Square Rebuses: each row `a op b op c op d = r` evaluated strictly left to right (`+ - x :`, `:` exact), numbers 1..999 without leading zero, column j sums to the result of row j, the summary row repeats the four results and totals them. Win = any grid that satisfies all rules. Tap a box, tap a digit; Check turns broken rules into black boxes; Del, Reset (tap twice), `<` `>` puzzles, List, `?` rules, Home. Keys: Back = leave screen/exit, Home or Menu key = straight to the home screen, Prev/Next = puzzle.
- Files on the drive: progress `/mnt/ext1/rebus.sav` and `/mnt/ext1/digit.sav` (one line per puzzle: solved flag, grid, ms timestamp of the last change; older lines without the timestamp still load), the transfer file `/mnt/ext1/puzzlebook-progress.json`, settings `/mnt/ext1/puzzlebook.cfg`, debug log `/mnt/ext1/puzzlebook.txt` (screen size, orientation, language setting and the language actually shown).
- Install: copy `reader/Puzzle Book.app` to `<drive>:\applications`, eject, open it from the Applications menu. Delete an old `Square Rebuses.app` there.

## Layout
- `native/puzzlebook/`: `shell.h` (module interface + shared helpers), `shell.c` (home, settings, routing, random), `i18n.c/.h` (languages, setting, `tr()`), `progress.c/.h` (progress file: JSON writer/reader, merge), `ui.c`, `modules.c` (registry), `rebus/` and `digit/` (modules), `example/` (template, not registered), `Makefile`, `README.md` (how to add a module).
- `tools/`: `gen-rebus-data.py` (module01.json -> `rebus_data.h`), `gen-digit-data.py` (module02.json -> `digit_data.h`), `fake-inkview/` (stand-in `inkview.h` that draws to SVG), `test-rebus-ui.sh`, `test-modules1.c`/`test-modules2.c` (test registries).
- `reader/Puzzle Book.app`: the built binary (marked binary in `.gitattributes`).

## Build
The SDK is not committed (about 1.7 GB): `git clone --depth 1 https://github.com/pocketbook/SDK_481 sdk` (default branch 5.5: gcc 4.8.1 i386-static cross compiler, glibc 2.9 sysroot, `libinkview.so`). Then:

```
make -C native/puzzlebook SDK=$PWD/sdk
```

writes `reader/Puzzle Book.app` (stripped). Check `readelf -V "reader/Puzzle Book.app"` shows only GLIBC_2.4 and `readelf -d` only `libinkview.so` and `libc.so.6`. In the cloud sandbox the compiler is a downloaded binary: the user must allow running it once.

## Test (sandbox, no reader needed)
- Texts are UTF-8 `Str` triples `S("English", "Русский", "Deutsch")` looked up with `tr()`; translations reuse the source app's `app_{en,ru,de}.arb` where a string exists there. Buttons shrink their font to fit long Russian/German words (`ui_draw_btn`).
- `make -C native/puzzlebook test`: Square Rebuses, 1024 checks of the rules (data consistent, givens locked, every single wrong digit breaks a rule, save/load, zero/leading zero, bad division); Digit Rebuses, 510 checks (glyph sets, evaluation without precedence, negative/zero/inexact steps, every printed answer wins and has exactly one solution by exhaustive search, glyph violations, save/load).
- `sh tools/test-rebus-ui.sh` (`OUT=dir` keeps SVG/PNG screenshots, PNG needs node + playwright): runs the real shell and module code against `tools/fake-inkview`: home, open module, solve all 16 puzzles through the buttons in portrait and landscape, Random (fresh and with one puzzle left), help, Transfer progress (export file content, load into a clean state, timestamps kept, merge rules with an app-style bundle containing unknown namespaces/puzzles/bad cells, bad/missing/newer files, ru/de, landscape), all 19 Digit Rebuses puzzles solved through the pad in both orientations plus errors/list/help/Home and Russian and German runs, Settings with Russian and German walk-throughs of home/game/list/help in both orientations, the saved language and "system" following `global.cfg`, Back key closes the app, and a second build with the template module registered (two tiles, routing both ways, separate save files).

## Working loop
Claude changes the code, runs the tests, rebuilds the binary and commits it. The user copies it to the reader, tries it, and pastes `puzzlebook.txt` plus a description of what the screen did.

## Next steps / ideas
- Test Transfer progress on the reader (save the file, copy it over USB, load it back / on the phone app).
- More modules from il721/puzzles: Domino Solitaire, Alphabet Labyrinth, Squarewords, Keyword Rebuses (see https://github.com/il721/puzzles `packages/module_*`). Add one per `native/puzzlebook/README.md`.
- Ideas: tutorial walkthrough for the sample puzzle (the original has 15 guided steps), hints, a dice/continue button per module, e-ink refresh tuning (more partial updates), module icons are not possible as app icons (see il721/PB625 notes) but could be drawn on the tiles.
