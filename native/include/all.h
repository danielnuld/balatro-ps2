// Every header of the port, in dependency order. A .c file includes this (and its own needs/<file>.h after it).
#ifndef ALL_H
#define ALL_H
#include "core.h"
#include "love.h"
#include "data.h"
#include "types.h"
#include "node.h"
#include "event.h"
#include "controller.h"
#include "ui.h"
#include "cardarea.h"
#include "card.h"
#include "blind.h"
#include "tag.h"
#include "back.h"
#include "card_character.h"
#include "challenges.h"
#include "game.h"
#include "io.h"
#include "misc_functions.h"
#include "common_events.h"
#include "state_events.h"
#include "button_callbacks.h"
#include "UI_definitions.h"

// The game's code takes its memory from the collector (gc.h): what the Lua left to its garbage collector is
// reclaimed there, and a block of these may hold pointers to tables, objects and strings (libc's memory is not
// scanned, so a pointer kept only there would not keep its block alive). free() frees a block at once, and
// still gives libc what is libc's (the buffers of love_filesystem_read and the like).
#define malloc(n) gc_alloc(n)
#define calloc(n, size) gc_alloc((size_t)(n) * (size))
#define realloc(p, n) gc_realloc(p, n)
#define strdup(s) gc_strdup(s)
#define free(p) gc_free(p)
#endif
