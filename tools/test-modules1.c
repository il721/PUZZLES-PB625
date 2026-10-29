/* Registry used by tools/test-rebus-ui.sh for the Square Rebuses-only tests (the real modules.c lists every module). */
#include "shell.h"
extern const Module module_rebus;
const Module *const MODULES[] = {&module_rebus};
const int NUM_MODULES = 1;
