#!/bin/sh
# Builds the app against the fake InkView for the build host (with ASan/UBSan unless SAN=0).
# Output: $OUT/tatham-host (default /tmp/tatham-host-build). Usage: sh test/build-host.sh
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT=${OUT:-/tmp/tatham-host-build}
SAN=${SAN:-1}
mkdir -p "$OUT/obj"
CORE="combi divvy draw-poly drawing dsf findloop grid latin laydomino loopgen malloc matching midend misc penrose penrose-legacy ps random sort tdq tree234 version hat spectre list"
GAMES=$(sed 's/GAME(\(.*\))/\1/' "$HERE/puzzles/generated-games.h")
SANFLAGS=""
VENDOR=""
[ "$SAN" = 1 ] && SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -g" && VENDOR="-fno-sanitize=shift-base"   # upstream shifts negative values (harmless)
FT=""
FTLIBS=""
if pkg-config --exists freetype2 2>/dev/null && [ -f /usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf ]; then
  FT="-DUSE_FT $(pkg-config --cflags freetype2)"; FTLIBS="$(pkg-config --libs freetype2)"   # real glyphs and metrics in the fake
fi
CF="-O1 -std=gnu11 $SANFLAGS -DCOMBINED -DTAT_DIR=\"$OUT/data\" -DSYSCFGF=\"$OUT/global.cfg\" -DTAT_ZIP=\"$OUT/tatham-saves.zip\" -I $HERE/test/fake -I $HERE/puzzles"
for f in $CORE $GAMES; do echo "$f"; done | xargs -P 8 -I{} sh -c \
  "[ \"$OUT/obj/{}.o\" -nt \"$HERE/puzzles/{}.c\" ] && [ \"$OUT/obj/{}.o\" -nt \"$OUT/stamp\" ] || gcc $CF -w $VENDOR -c $HERE/puzzles/{}.c -o $OUT/obj/{}.o"
for f in main fe tr rules savezip; do
  gcc $CF -Wall -Wextra -Wno-unused-parameter $( [ "$f" = main ] && echo -Dmain=app_main ) -c "$HERE/$f.c" -o "$OUT/obj/app_$f.o"
done
gcc $CF $FT -Wall -c "$HERE/test/fake_inkview.c" -o "$OUT/obj/fake.o"
gcc $SANFLAGS -o "$OUT/tatham-host" "$OUT"/obj/*.o -lm $FTLIBS
touch "$OUT/stamp"
echo "$OUT/tatham-host"
