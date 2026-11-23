// Declarations src/main.c needs that no shared header has.
#ifndef NEEDS_MAIN_H
#define NEEDS_MAIN_H
#include "node.h"

// main.lua:2-29 (the requires): node.h:28 mentions classes_init() but no shared header declares it (src/object.c
// exports it, see needs/object.h). Fills every class's inherited methods; called once before any object is made.
void classes_init(void);
#endif
