# Build "Puzzle Book.app" for the PocketBook 625 with the PocketBook SDK 4.8.1
# (github.com/pocketbook/SDK_481, branch 5.5: gcc 4.8.1, glibc 2.9, libinkview).
#   make SDK=/path/to/SDK_481          device binary -> ../../reader/Puzzle Book.app
#   make test                          host test of the module rules
# Adding a module: put its .c files in MODSRC and register it in modules.c.
SDK ?= $(CURDIR)/sdk
CC  = $(SDK)/bin/arm-obreey-linux-gnueabi-gcc
OUT = ../../reader/Puzzle Book.app
CFLAGS = -O2 -Wall -Wextra -std=gnu99

SHELLSRC = shell.c ui.c i18n.c modules.c
MODSRC   = rebus/module_rebus.c rebus/rebus_core.c

all:
	$(CC) $(CFLAGS) $(SHELLSRC) $(MODSRC) -o "$(OUT)" -linkview
	$(SDK)/bin/arm-obreey-linux-gnueabi-strip "$(OUT)"

rebus/rebus_data.h: rebus/module01.json ../../tools/gen-rebus-data.py
	python3 ../../tools/gen-rebus-data.py

test: rebus/test_core.c rebus/rebus_core.c rebus/rebus_core.h rebus/rebus_data.h
	gcc -Wall -Wextra -o /tmp/rebus_test rebus/test_core.c rebus/rebus_core.c && /tmp/rebus_test

.PHONY: all test
