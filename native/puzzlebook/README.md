# Puzzle Book (native inkview app for the PocketBook 625)

`reader/Puzzle Book.app` is the envelope: a home screen with one tile per module (name, "solved / total",
description), a **Random** button (random unsolved puzzle from any module, weighted by how many each module has
left), a help page and **Exit**. Opening a tile hands every event to that module until the module calls
`shell_leave()` (its **Home** button or the Back key). It mirrors the shell in github.com/il721/puzzles
(`moduleCatalog` = `modules.c`, `PuzzleModuleDescriptor` = `Module` in `shell.h`).

Files: `shell.h` (module interface + shared helpers), `shell.c` (home, routing, random), `ui.c` (fonts, boxes,
buttons, log), `modules.c` (registry), `rebus/` (module "Square Rebuses"), `digit/` ("Digit Rebuses"), `domino/` ("Domino Solitaire"), `example/` (template, not registered).

## Add a module
1. Copy `example/` to `native/puzzlebook/<name>/` and implement `const Module module_<name>` (`shell.h` documents
   each callback; `title`/`description` are `Str` triples `S("English", "Русский", "Deutsch")`, and every on-screen text of the module
   should be one too, shown with `tr()` so the language setting applies): `init` loads the save file, `enter(puzzle)` opens it (-1 = resume) and must draw, `event`
   gets `EVT_SHOW`, `EVT_ORIENTATION`, `EVT_POINTERUP`, `EVT_KEYPRESS`, `save`, `puzzle_count`/`solved_count`
   (tutorials not counted) and `pick_random`.
2. `modules.c`: add `extern const Module module_<name>;` and `&module_<name>` to `MODULES[]` (tile order).
3. `Makefile`: add its `.c` files to `MODSRC`.
   Progress file: to take part in Settings > Transfer progress, fill the last five `Module` fields (`ns` = the source app's namespace such as `module_rebus`, `prog_count/id/get/put`) and keep a per-puzzle `ui_now_ms()` stamp that you set on every change and save after the grid; see `rebus/module_rebus.c`. Modules without `ns` are left out of the file. An entry is either `cells` (key -> digit, `ProgEntry.cells`) or, like the source's Domino module, a `placed` list of dominoes (`ProgEntry.dom`, see `domino/module_domino.c`); `progress.c` zeroes the entry before every `prog_get`, so set only what you use.
4. Save file: `/mnt/ext1/<id>.sav` (own format; keep it a plain text file). Use `atoi`/`strtol`, not `sscanf`/`fscanf`
   (they need GLIBC_2.7; the firmware's libc may be older). Only call inkview functions already used here
   unless you check the device has them (`readelf -Ws --dyn-syms` on the binary lists what it imports).
5. Build and test: `make -C native/puzzlebook SDK=/path/to/SDK_481`, `sh tools/test-rebus-ui.sh`
   (add a module to `tools/test-modules2.c`-style registry to play it in the host harness).
