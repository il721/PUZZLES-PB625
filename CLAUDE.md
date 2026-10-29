# PUZZLES-PB625

Native puzzle games for the PocketBook 625 e-reader (Puzzle Book app with modules). Read README.md first: it holds the device facts, how to build, how to test and the next steps.

Rules:
- Claude writes all code. The user runs the app on the PocketBook 625 (copies `reader/Puzzle Book.app` to `<drive>:\applications` over USB) and pastes `puzzlebook.txt` and what the screen did back; this sandbox has no USB or LAN access to the reader.
- The app is a native ARM binary built with the PocketBook SDK 4.8.1 (see README.md "Build"). Keep the binary at GLIBC_2.4 only: no `sscanf`/`fscanf` (use `atoi`/`strtol`), and check `readelf -V "reader/Puzzle Book.app"` after every build. Only add inkview calls after checking the device has them.
- Always run `sh tools/test-rebus-ui.sh` and `make -C native/puzzlebook test` before committing, and rebuild `reader/Puzzle Book.app` when native code changed (commit the binary).
- Running the SDK compiler (a downloaded binary) needs the user's permission in the cloud sandbox: ask once, then go ahead.
- Never commit Wi-Fi or server passwords. Wi-Fi and the file transfer tools live in the separate repo il721/PB625.
- Answer in English.
