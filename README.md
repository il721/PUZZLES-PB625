# PUZZLES-PB625: puzzle games for the PocketBook 625

`reader/Puzzle Book.app` is a native touch app (ARM ELF, PocketBook inkview) with a home screen and puzzle **modules**. First module: **Square Rebuses**, a port of `packages/module_rebus` from https://github.com/il721/puzzles (15 puzzles + the "Example" sample; `native/puzzlebook/rebus/module01.json` is that repo's puzzle data). Moved here from il721/PB625 on 2026-10-29 (clean first commit); Wi-Fi, file transfer and the Total Commander plugin stay in il721/PB625.

## Device facts
- PocketBook 625 (Basic Touch 2), firmware R4.4.1585, Linux 3.0.8 armv7l, 6" e-ink touch. Screen 600x800 portrait / 800x600 landscape; the app lays out for both (`EVT_ORIENTATION`).
- Apps run as uid 102 `sreader`, no root. The internal storage `/mnt/ext1` (vfat, no exec bit) is the USB drive; while USB is connected it is unmounted on the reader. Apps live in `<drive>:\applications\*.app`.
- Native apps link against the firmware's `libinkview.so` and libc. The SDK 4.8.1 toolchain targets glibc 2.9; the binary here needs only GLIBC_2.4.
- Confirmed by the user on the reader (2026-10-29): the Puzzle Book build with home screen + Square Rebuses works. The build with Settings and the three languages was tested only in the sandbox. To confirm on the device: Cyrillic text renders (UTF-8 in `DrawString`, LiberationSans), and "System default" picks the reader's language (see the `language setting` line in `puzzlebook.txt`).

## What the app does
- Home: one tile per module (name, "solved / total", description), **Random** (random unsolved puzzle from any module, weighted by how many each module has left), `?` help, **Exit**, **Settings** (bottom left; Menu key too).
- Languages and Settings (ported from il721/puzzles, same set: Русский, English, Deutsch): Settings has one section, Language: System default / Русский / English / Deutsch. "System default" reads `language=xx` from the firmware's `/mnt/ext1/system/config/global.cfg` (unknown or missing -> English, like the source). The choice is saved in `/mnt/ext1/puzzlebook.cfg` (`lang=system|ru|en|de`). Not ported: sound (the reader has no speaker), dark theme (not sensible on e-ink, would need inkview invert calls not yet checked on the device), progress export/import (progress is a plain file on the drive).
- Opening a tile hands all events to the module until it calls `shell_leave()` (Home button or Back key).
- Square Rebuses: each row `a op b op c op d = r` evaluated strictly left to right (`+ - x :`, `:` exact), numbers 1..999 without leading zero, column j sums to the result of row j, the summary row repeats the four results and totals them. Win = any grid that satisfies all rules. Tap a box, tap a digit; Check turns broken rules into black boxes; Del, Reset (tap twice), `<` `>` puzzles, List, `?` rules, Home. Keys: Back = leave screen/exit, Menu = list, Prev/Next = puzzle.
- Files on the drive: progress `/mnt/ext1/rebus.sav` (one line per puzzle, kept from earlier builds), settings `/mnt/ext1/puzzlebook.cfg`, debug log `/mnt/ext1/puzzlebook.txt` (screen size, orientation, language setting and the language actually shown).
- Install: copy `reader/Puzzle Book.app` to `<drive>:\applications`, eject, open it from the Applications menu. Delete an old `Square Rebuses.app` there.

## Layout
- `native/puzzlebook/`: `shell.h` (module interface + shared helpers), `shell.c` (home, settings, routing, random), `i18n.c/.h` (languages, setting, `tr()`), `ui.c`, `modules.c` (registry), `rebus/` (module), `example/` (template, not registered), `Makefile`, `README.md` (how to add a module).
- `tools/`: `gen-rebus-data.py` (module01.json -> `rebus_data.h`), `fake-inkview/` (stand-in `inkview.h` that draws to SVG), `test-rebus-ui.sh`, `test-modules2.c`.
- `reader/Puzzle Book.app`: the built binary (marked binary in `.gitattributes`).

## Build
The SDK is not committed (about 1.7 GB): `git clone --depth 1 https://github.com/pocketbook/SDK_481 sdk` (default branch 5.5: gcc 4.8.1 i386-static cross compiler, glibc 2.9 sysroot, `libinkview.so`). Then:

```
make -C native/puzzlebook SDK=$PWD/sdk
```

writes `reader/Puzzle Book.app` (stripped). Check `readelf -V "reader/Puzzle Book.app"` shows only GLIBC_2.4 and `readelf -d` only `libinkview.so` and `libc.so.6`. In the cloud sandbox the compiler is a downloaded binary: the user must allow running it once.

## Test (sandbox, no reader needed)
- Texts are UTF-8 `Str` triples `S("English", "Русский", "Deutsch")` looked up with `tr()`; translations reuse the source app's `app_{en,ru,de}.arb` where a string exists there. Buttons shrink their font to fit long Russian/German words (`ui_draw_btn`).
- `make -C native/puzzlebook test`: 1024 checks of the rules (data consistent, givens locked, every single wrong digit breaks a rule, save/load, zero/leading zero, bad division).
- `sh tools/test-rebus-ui.sh` (`OUT=dir` keeps SVG/PNG screenshots, PNG needs node + playwright): runs the real shell and module code against `tools/fake-inkview`: home, open module, solve all 16 puzzles through the buttons in portrait and landscape, Random (fresh and with one puzzle left), help, Settings with Russian and German walk-throughs of home/game/list/help in both orientations, the saved language and "system" following `global.cfg`, Back key closes the app, and a second build with the template module registered (two tiles, routing both ways, separate save files).

## Working loop
Claude changes the code, runs the tests, rebuilds the binary and commits it. The user copies it to the reader, tries it, and pastes `puzzlebook.txt` plus a description of what the screen did.

## Next steps / ideas
- Test the current build on the reader (Settings, Russian/German text, home screen layout, Random, Home button).
- More modules from il721/puzzles: Digit Rebuses, Domino Solitaire, Alphabet Labyrinth, Squarewords, Keyword Rebuses (see https://github.com/il721/puzzles `packages/module_*`). Add one per `native/puzzlebook/README.md`.
- Ideas: tutorial walkthrough for the sample puzzle (the original has 15 guided steps), hints, a dice/continue button per module, e-ink refresh tuning (more partial updates), module icons are not possible as app icons (see il721/PB625 notes) but could be drawn on the tiles.
