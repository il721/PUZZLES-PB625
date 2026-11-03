# Build "Puzzle Collection.app" (Simon Tatham's puzzles) for the PocketBook 625 with the PocketBook SDK 4.8.1
# (github.com/pocketbook/SDK_481, branch 5.5: gcc 4.8.1, glibc 2.9, libinkview).
#   make -j8 SDK=/path/to/SDK_481      device binary -> ../../reader/Puzzle Collection.app
#   make test                          host tests (fake InkView, ASan/UBSan): sh test/test.sh
# The binary must need only GLIBC_2.4: check `readelf -V "../../reader/Puzzle Collection.app"`.
SDK ?= $(CURDIR)/../../sdk
CC  = $(SDK)/bin/arm-obreey-linux-gnueabi-gcc
STRIP = $(SDK)/bin/arm-obreey-linux-gnueabi-strip
OUT = ../../reader/Puzzle Collection.app
OBJ = build

# -D_GNU_SOURCE: glibc 2.9 headers would otherwise map sscanf to __isoc99_sscanf (GLIBC_2.7, missing on the reader)
COMMON = -O2 -std=gnu99 -D_GNU_SOURCE -DCOMBINED -I puzzles
CORE   = combi divvy draw-poly drawing dsf findloop grid latin laydomino loopgen malloc matching midend misc penrose penrose-legacy ps random sort tdq tree234 version hat spectre list
GAMES  = $(shell sed 's/GAME(\(.*\))/\1/' puzzles/generated-games.h)
APP    = main fe tr rules

PUZOBJ = $(addprefix $(OBJ)/p_,$(addsuffix .o,$(CORE) $(GAMES)))
APPOBJ = $(addprefix $(OBJ)/a_,$(addsuffix .o,$(APP)))

# make cannot take a target with a space in it, so link to $(OBJ)/puzzlecollection and copy
all: $(OBJ)/puzzlecollection
	cp $< "$(OUT)"
	$(STRIP) "$(OUT)"

$(OBJ)/puzzlecollection: $(PUZOBJ) $(APPOBJ)
	$(CC) -o $@ $^ -linkview -lm

# vendored upstream code: no warnings from it
$(OBJ)/p_%.o: puzzles/%.c | $(OBJ)
	$(CC) $(COMMON) -w -c $< -o $@

$(OBJ)/a_%.o: %.c tat.h tr.h | $(OBJ)
	$(CC) $(COMMON) -Wall -Wextra -Wno-unused-parameter -c $< -o $@

$(OBJ):
	mkdir -p $(OBJ)

test:
	sh test/test.sh

clean:
	rm -rf $(OBJ)

.PHONY: all test clean
