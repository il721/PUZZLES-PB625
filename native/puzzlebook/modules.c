/* The module registry: the one place the shell learns about a module (like moduleCatalog in
 * il721/puzzles). Tiles on the home screen appear in this order. */
#include "shell.h"

extern const Module module_rebus;
extern const Module module_digit;
extern const Module module_domino;
extern const Module module_labyrinth;
/* extern const Module module_xxx; */

const Module *const MODULES[] = {
    &module_rebus,
    &module_digit,
    &module_domino,
    &module_labyrinth,
    /* &module_xxx, */
};
const int NUM_MODULES = (int)(sizeof(MODULES) / sizeof(MODULES[0]));
