// The game's files. In development they are a directory (host:game through the emulator, build/game on
// the host). On a disc they are one file, the pack tools/disc.py writes: a disc of hundreds of small files with
// long names is slow to open and more than ISO 9660 level 1 allows. A path that starts with "pak:/" is a file of
// the mounted pack; anything else is a file of its own.
//
// The pack: "BPAK", uint32 count, then per file uint32 offset, uint32 size, uint16 name length and the name, sorted
// by name; then the files, each from a multiple of 2048 bytes (a sector).
//
// Reads of the pack go through one descriptor kept open, each a single request of its whole size: opening the
// pack again for every file and reading it through stdio's 1 KB buffer made the boot from the disc 22 s, and the
// disc's file system ran out of open files halfway through a run.
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pack.h"

#ifdef _EE
#include <kernel.h>
#include <libcdvd.h>
static int lock = -1; // one request at a time: the sounds' streams are read from a thread of their own (snd.c)
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct { const char *name; uint32_t offset, size; } Entry;
static Entry *entries;
static uint32_t count;
static int pack_fd = -1;

static int by_name(const void *a, const void *b) { return strcmp(((const Entry *)a)->name, ((const Entry *)b)->name); }

static Entry *find(const char *path) {
    Entry key = {path + 5, 0, 0};
    return entries ? bsearch(&key, entries, count, sizeof key, by_name) : NULL;
}
static int packed(const char *path) { return !strncmp(path, "pak:/", 5); }

int pack_mount(const char *path) {
#ifdef _EE
    sceCdInit(SCECdINoD);
    sceCdMmode(SCECdMmodeDvd);
#endif
    uint32_t head[2];
    pack_fd = open(path, O_RDONLY | O_BINARY);
    if (pack_fd < 0 || read(pack_fd, head, 8) != 8 || memcmp(head, "BPAK", 4)) return 0;
    count = head[1];
    // the index is in the first sectors: one read (it is under 16 KB for the game's 200 files)
    uint32_t first;
    if (read(pack_fd, &first, 4) != 4 || first > 1 << 20) return 0;
    unsigned char *index = malloc(first + 1);
    entries = malloc(count * sizeof *entries);
    if (!index || !entries || lseek(pack_fd, 0, SEEK_SET) != 0 || read(pack_fd, index, first) != (long)first) return 0;
    unsigned char *p = index + 8;
    for (uint32_t i = 0; i < count; i++) { // the names stay in the index, each ended over the next entry's first byte
        uint32_t at[2];
        uint16_t len;
        memcpy(at, p, 8), memcpy(&len, p + 8, 2);
        if (i) p[0] = 0;
        entries[i] = (Entry){(char *)p + 10, at[0], at[1]};
        p += 10 + len;
    }
    p[0] = 0; // (the first file starts on the next sector at the earliest, so this byte is the index's or padding)
    return 1;
}

long pack_size(const char *path) {
    if (packed(path)) {
        Entry *e = find(path);
        return e ? (long)e->size : -1;
    }
    int fd = open(path, O_RDONLY | O_BINARY);
    if (fd < 0) return -1;
    long n = lseek(fd, 0, SEEK_END);
    close(fd);
    return n;
}

int pack_read(const char *path, long offset, void *to, long n) {
    Entry *e = packed(path) ? find(path) : NULL;
    if (packed(path) && (!e || offset + n > (long)e->size)) return 0;
#ifdef _EE
    if (lock < 0) {
        ee_sema_t sema = {.init_count = 1, .max_count = 1};
        lock = CreateSema(&sema);
    }
    WaitSema(lock);
#endif
    int fd = e ? pack_fd : open(path, O_RDONLY | O_BINARY), ok = 0;
    if (fd >= 0) ok = lseek(fd, (e ? (long)e->offset : 0) + offset, SEEK_SET) >= 0 && read(fd, to, n) == n;
    if (fd >= 0 && !e) close(fd);
#ifdef _EE
    SignalSema(lock);
#endif
    return ok;
}
