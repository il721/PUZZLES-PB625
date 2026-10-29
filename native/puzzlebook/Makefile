# Build "Puzzle Book.app" for the PocketBook 625 with the PocketBook SDK 4.8.1
# (github.com/pocketbook/SDK_481, branch 5.5: gcc 4.8.1, glibc 2.9, libinkview).
#   make SDK=/path/to/SDK_481          device binary -> ../../reader/Puzzle Book.app
#   make test                          host test of the module rules
# Adding a module: put its .c files in MODSRC and register it in modules.c.
SDK ?= $(CURDIR)/sdk
CC  = $(SDK)/bin/arm-obreey-linux-gnueabi-gcc
OUT = ../../reader/Puzzle Book.app
CFLAGS = -O2 -Wall -Wextra -std=gnu99

SHELLSRC = shell.c ui.c i18n.c progress.c modules.c
MODSRC   = rebus/module_rebus.c rebus/rebus_core.c digit/module_digit.c digit/digit_core.c domino/module_domino.c domino/domino_core.c labyrinth/module_labyrinth.c labyrinth/labyrinth_core.c squareword/module_squareword.c squareword/squareword_core.c keyword/module_keyword.c keyword/keyword_core.c

all:
	$(CC) $(CFLAGS) $(SHELLSRC) $(MODSRC) -o "$(OUT)" -linkview
	$(SDK)/bin/arm-obreey-linux-gnueabi-strip "$(OUT)"

rebus/rebus_data.h: rebus/module01.json ../../tools/gen-rebus-data.py
	python3 ../../tools/gen-rebus-data.py

digit/digit_data.h: digit/module02.json ../../tools/gen-digit-data.py
	python3 ../../tools/gen-digit-data.py

domino/domino_data.h: domino/module03.json ../../tools/gen-domino-data.py
	python3 ../../tools/gen-domino-data.py

labyrinth/labyrinth_data.h: labyrinth/module04.json ../../tools/gen-labyrinth-data.py
	python3 ../../tools/gen-labyrinth-data.py

squareword/squareword_data.h: squareword/module05.json ../../tools/gen-squareword-data.py
	python3 ../../tools/gen-squareword-data.py

keyword/keyword_data.h: keyword/module07.json ../../tools/gen-keyword-data.py
	python3 ../../tools/gen-keyword-data.py

test: rebus/test_core.c rebus/rebus_core.c rebus/rebus_core.h rebus/rebus_data.h digit/test_core.c digit/digit_core.c digit/digit_core.h digit/digit_data.h domino/test_core.c domino/domino_core.c domino/domino_core.h domino/domino_data.h labyrinth/test_core.c labyrinth/labyrinth_core.c labyrinth/labyrinth_core.h labyrinth/labyrinth_data.h squareword/test_core.c squareword/squareword_core.c squareword/squareword_core.h squareword/squareword_data.h keyword/test_core.c keyword/keyword_core.c keyword/keyword_core.h keyword/keyword_data.h
	gcc -Wall -Wextra -o /tmp/rebus_test rebus/test_core.c rebus/rebus_core.c && /tmp/rebus_test
	gcc -Wall -Wextra -o /tmp/digit_test digit/test_core.c digit/digit_core.c && /tmp/digit_test
	gcc -Wall -Wextra -o /tmp/domino_test domino/test_core.c domino/domino_core.c && /tmp/domino_test
	gcc -Wall -Wextra -O2 -o /tmp/labyrinth_test labyrinth/test_core.c labyrinth/labyrinth_core.c && /tmp/labyrinth_test
	gcc -Wall -Wextra -O2 -o /tmp/squareword_test squareword/test_core.c squareword/squareword_core.c && /tmp/squareword_test
	gcc -Wall -Wextra -O2 -o /tmp/keyword_test keyword/test_core.c keyword/keyword_core.c && /tmp/keyword_test

.PHONY: all test
