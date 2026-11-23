// Declarations src/object.c exports that no shared header has.
#ifndef NEEDS_OBJECT_H
#define NEEDS_OBJECT_H
#include "node.h"

// node.h:28 mentions it but does not declare it. Calls class_init for every class the headers declare
// ("extern Class"); call it once at start, before any object is made.
void classes_init(void);
#endif
