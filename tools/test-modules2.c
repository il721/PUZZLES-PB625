/* Registry used by tools/test-rebus-ui.sh to check the shell with two modules. */
#include "shell.h"
extern const Module module_rebus, module_example;
const Module *const MODULES[] = {&module_rebus, &module_example};
const int NUM_MODULES = 2;
