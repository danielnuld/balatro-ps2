// love.filesystem and love.data: the game's files (a directory, or the pack of a disc) and the saves.
//
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
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <sys/stat.h>
#include <zlib.h>

#define LOVE_INTERNAL
#include "love.h"

#ifdef _EE
#include <delaythread.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libmc.h>
#include <loadfile.h>
static int lock = -1; // one request at a time: the sounds' streams are read from a thread of their own (love_snd.c)
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

// ---------------------------------------------------------------- the saves
// The files the game writes: settings, profiles and the run in progress, and the deflate of love.data, which the
// game puts them through. They live in one directory: mc0:/BALATRO on the EE, any directory on the host.
// A file on the card is 12 bytes ("BLT1", the length, its Adler-32) and then the data: a file cut short by a
// power off reads as no file at all. The game itself cannot take one (it finds save.jkr, fails to inflate it and
// indexes nil, button_callbacks.lua:205).
// ponytail: a write goes straight over the old file, so that power cut loses the file (and on the EE the write
// comes some time after the game asked for it: the card's thread, below). A temporary file and a rename if it hurts.
// ponytail: no icon.sys, so the console's browser shows the directory as corrupted data; not done yet.
static char base[200];
static int card; // the saves' directory can be written: else (and with no directory) the saves stay in memory

// -DSLOW_CARD=<ms> (EE): every write, removal and directory made takes that much longer, as the console's card over
// the network does (seconds), to see that the game does not wait for them
#if defined(_EE) && defined(SLOW_CARD)
#define SLOW() DelayThread(SLOW_CARD * 1000)
#else
#define SLOW()
#endif

// (one buffer: on the EE every call is made holding card_lock, or before the card's thread starts)
static const char *full(const char *name) {
    static char path[300];
    snprintf(path, sizeof path, "%s/%s", base, name);
    return path;
}

static void save_remove(const char *name);
static int save_open(const char *dir) {
    snprintf(base, sizeof base, "%s", dir);
#ifdef _EE
    // love_pad.c has loaded rom0:XSIO2MAN; with these two, mc0: is a device like any other
    SifLoadModule("rom0:XMCMAN", 0, NULL);
    SifLoadModule("rom0:XMCSERV", 0, NULL);
#endif
    mkdir(base, 0777); // it may exist
    FILE *f = fopen(full("probe"), "wb");
    if (f) fclose(f), save_remove("probe");
    return f != NULL;
}

static void header(unsigned char h[12], const char *data, size_t len) {
    unsigned long sum = adler32(adler32(0, NULL, 0), (const Bytef *)data, len);
    memcpy(h, "BLT1", 4);
    for (int i = 0; i < 4; i++) h[4 + i] = len >> (8 * i), h[8 + i] = sum >> (8 * i);
}

// A file of the card, a 0 after it; NULL if it is not there, or not whole
static char *save_read(const char *name, size_t *size) {
    FILE *f = fopen(full(name), "rb");
    if (!f) return NULL;
    unsigned char h[12], want[12];
    size_t len = fread(h, 1, 12, f) == 12 && !memcmp(h, "BLT1", 4) ? h[4] | h[5] << 8 | h[6] << 16 | (size_t)h[7] << 24 : 0;
    char *data = len <= 4 << 20 ? malloc(len + 1) : NULL; // (4 MB: more than the card holds)
    int ok = data && fread(data, 1, len, f) == len;
    fclose(f);
    if (ok) header(want, data, len), ok = !memcmp(h, want, 12);
    if (!ok) return free(data), NULL;
    data[len] = 0, *size = len;
    return data;
}

static int save_write(const char *name, const char *data, size_t len) {
    unsigned char h[12];
    header(h, data, len);
    FILE *f = fopen(full(name), "wb");
    SLOW();
    int ok = f && fwrite(h, 1, 12, f) == 12 && fwrite(data, 1, len, f) == len;
    if (f && fclose(f)) ok = 0;
    return ok;
}

static void save_remove(const char *name) {
    const char *path = full(name);
    SLOW();
    remove(path);
#ifdef _EE
    // The console's own file module removes the file and then, falling through, makes a directory of that name
    // (seen in PCSX2: the name could never be written again). Removing that directory is the known way around.
    rmdir(path);
#endif
}

// one pass of zlib over the data, the output grown as it fills; NULL if it does not end as a stream should
static void *flate(const void *data, size_t size, size_t *out_size, z_stream *z, int (*step)(z_stream *, int), int (*end)(z_stream *)) {
    size_t room = size * 2 + 256, len = 0;
    unsigned char *out = malloc(room);
    z->next_in = (Bytef *)data, z->avail_in = size;
    int r = Z_MEM_ERROR;
    while (out) {
        z->next_out = out + len, z->avail_out = room - len - 1; // (a byte for the 0 after the data)
        r = step(z, Z_FINISH);
        len = room - 1 - z->avail_out;
        if (!(r == Z_OK || (r == Z_BUF_ERROR && z->avail_out == 0))) break;
        unsigned char *more = realloc(out, room *= 2);
        if (!more) free(out), r = Z_MEM_ERROR;
        out = more;
    }
    end(z);
    if (r != Z_STREAM_END) return free(out), NULL;
    out[len] = 0, *out_size = len;
    return out;
}
// LÖVE's "deflate" is the raw format; the game only asks for strings (string_packer.lua)
void *love_data_compress(const void *data, size_t size, int level, size_t *out_size) {
    z_stream z = {0};
    if (deflateInit2(&z, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) love_error("save: no memory to deflate");
    void *out = flate(data, size, out_size, &z, deflate, deflateEnd);
    if (!out) love_error("save: no memory to deflate");
    return out;
}
void *love_data_decompress(const void *data, size_t size, size_t *out_size) {
    z_stream z = {0};
    if (inflateInit2(&z, -15) != Z_OK) love_error("save: no memory to inflate");
    return flate(data, size, out_size, &z, inflate, inflateEnd);
}

// ---------------------------------------------------------------- love.filesystem
// The game's files come from files.txt (written by tools/extract.py): no directory calls needed on any device.
// What the game writes goes to the saves' directory when there is one (LoveConf.save_dir); otherwise it stays in
// memory, so benches and tests start every time from a new profile.
static char game_dir[200];
static struct { char *path; long size; } *files;
static int nfiles;
// What is known of each save: its contents, or that it is not there (the card is slow to ask)
// (dir: a directory made or found there. failed: set by the card's thread when its write did not go through)
typedef struct Saved { struct Saved *next; char *path, *data; size_t size; int known, dir; volatile int failed; } Saved;
static Saved *saves;
static int listed; // the card's files were all read at the start: a save not in `saves` is not on the card

const char *love_game_path(const char *path) {
    static char full_path[400];
    snprintf(full_path, sizeof full_path, "%s/%s", game_dir, path);
    return full_path;
}
const char *love_filesystem_getSourceBaseDirectory(void) { return game_dir; }

static Saved *entry(const char *path), *saved(const char *path);
static void save_list(const char *dir);
#ifdef _EE
static void card_start(void);
#endif
void love_fs_init(const char *game, const char *save_dir) {
    snprintf(game_dir, sizeof game_dir, "%s", game);
    const char *list = love_game_path("files.txt");
    long size = pack_size(list);
    char *text = size > 0 ? malloc(size + 1) : NULL;
    if (!text || !pack_read(list, 0, text, size)) love_error("no game files in %s", game);
    text[size] = 0;
    for (char *l = text; *l; l++) nfiles += *l == '\n';
    if (!(files = calloc(nfiles + 1, sizeof *files))) love_error("no memory for the game's file list");
    nfiles = 0;
    for (char *l = strtok(text, "\r\n"); l; l = strtok(NULL, "\r\n")) { // "<size> <path>"; the paths stay in text
        char *path;
        long n = strtol(l, &path, 10);
        if (path == l || *path != ' ') continue;
        files[nfiles].path = path + 1, files[nfiles++].size = n;
    }
    if (save_dir) {
        card = save_open(save_dir);
        printf("SAVE %s %s\n", save_dir, card ? "ready" : "CANNOT BE WRITTEN: nothing will be saved");
        if (card) save_list("");
#ifdef _EE
        if (card) card_start();
#endif
    }
}

static long game_file(const char *path) { // its size, or -1
    for (int i = 0; i < nfiles; i++)
        if (!strcmp(files[i].path, path)) return files[i].size;
    return -1;
}
static int game_dir_has(const char *path) { // a directory of the game: some file is under it
    size_t len = strlen(path);
    if (!len) return 1;
    for (int i = 0; i < nfiles; i++)
        if (!strncmp(files[i].path, path, len) && files[i].path[len] == '/') return 1;
    return 0;
}

#ifdef _EE
// The card's thread. A write to the card takes seconds on the console (a virtual card over the network: 8 to 11 s
// for the files of one save, measured), so the game only leaves it in a queue: store() has put the contents in
// `saves`, where every read is answered from, and this thread deflates and writes them while the game goes on. A
// newer write of a file takes the place of one still waiting; everything else goes in the order it was asked.
// The thread runs above the game, which never sleeps (it spins in gs_present until the vertical blank: a thread
// below it would never run), and sleeps in the card's requests. The deflate is the one thing that costs it the
// EE's time (10 ms for a run, estimated): it goes in steps with a nap after each.
// card_lock is held around every call to the card: its modules take one at a time. The queue itself is touched
// with interrupts off. The thread sees nothing of the collector's memory: a job's data is a copy in libc's.
typedef struct Job { struct Job *next; Saved *s; char *data; size_t size; char what, deflate; double at; } Job; // what: 'w'rite, 'r'emove, make a 'd'irectory
static Job *jobs; // waiting: the one being done is out of the list
static int card_lock = -1, card_wake = -1;

static void card_queue(Saved *s, int what, const void *data, size_t size, int deflate) {
    Job *job = calloc(1, sizeof *job), *last = NULL, **end;
    char *copy = what == 'w' ? malloc(size + 1) : NULL, *old = NULL;
    if (!job || (what == 'w' && !copy)) love_error("save: no memory");
    if (copy) memcpy(copy, data, size);
    *job = (Job){NULL, s, copy, size, what, deflate, love_timer_getTime()};
    DI();
    for (end = &jobs; *end; end = &(*end)->next)
        if ((*end)->s == s) last = *end;
    if (what == 'w' && last && last->what == 'w') old = last->data, last->data = copy, last->size = size, last->deflate = deflate;
    else *end = job;
    EI();
    if (old) free(old), free(job);
    SignalSema(card_wake);
}
static Job *card_next(void) {
    DI();
    Job *job = jobs;
    if (job) jobs = job->next;
    EI();
    return job;
}
static int card_waiting(const Saved *s) {
    DI();
    const Job *job = jobs;
    while (job && job->s != s) job = job->next;
    EI();
    return job != NULL;
}
// What love_data_compress gives at level 1 (string_packer.lua:70), a step of 4 KB at a time
static char *deflate_steps(const char *data, size_t size, size_t *out_size, int naps) {
    z_stream z = {0};
    if (deflateInit2(&z, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return NULL;
    size_t room = deflateBound(&z, size);
    unsigned char *out = malloc(room);
    int r = out ? Z_OK : Z_MEM_ERROR;
    z.next_out = out, z.avail_out = room;
    for (size_t at = 0, n; r == Z_OK; at += n) {
        n = size - at < 4096 ? size - at : 4096;
        z.next_in = (Bytef *)data + at, z.avail_in = n;
        r = deflate(&z, at + n == size ? Z_FINISH : Z_NO_FLUSH);
        if (naps && r == Z_OK) DelayThread(8000); // (half a frame: a step is under a millisecond of the EE)
    }
    deflateEnd(&z);
    if (r != Z_STREAM_END) return free(out), NULL;
    *out_size = z.total_out;
    return (char *)out;
}
// The writes go to the card's own server on the IOP (libmc), not through the files' one as fopen does: that one
// serves a request at a time, so a printf (the console's log goes over the network), a texture read from the disc
// and the music's stream waited behind each request to the card (seen in PCSX2: the frame still stopped 100 ms
// per file). mcSync(0) spins, so the answer is asked for between naps. If libmc does not answer, or a write
// through it fails, the file goes the old way.
static int mc; // libmc answers for the saves' card
static int mc_wait(int sent) { // the result of the request just sent, negative if it failed
    int result = -1;
    if (sent) return -1;
    while (!mcSync(1, NULL, &result)) DelayThread(2000);
    return result;
}
static const char *mc_path(const char *name) { return strchr(full(name), ':') + 1; }
static int mc_write(const char *name, const char *data, size_t len) {
    unsigned char h[12];
    header(h, data, len);
    int fd = mc_wait(mcOpen(base[2] - '0', 0, mc_path(name), 0x602)); // (the IOP's O_WRONLY | O_CREAT | O_TRUNC)
    if (fd < 0) return 0;
    SLOW();
    int ok = mc_wait(mcWrite(fd, h, 12)) == 12 && mc_wait(mcWrite(fd, data, len)) == (int)len;
    return mc_wait(mcClose(fd)) >= 0 && ok;
}
static void card_do(Job *job, int naps) { // (holding card_lock)
    const char *path = job->s->path;
    double from = love_timer_getTime();
    if (job->what == 'd') {
        SLOW();
        if (mc) mc_wait(mcMkDir(base[2] - '0', 0, mc_path(path)));
        else mkdir(full(path), 0777);
    } else if (job->what == 'r') {
        if (!mc) save_remove(path);
        else {
            SLOW();
            mc_wait(mcDelete(base[2] - '0', 0, mc_path(path))); // (a file, or a directory left empty)
        }
    } else {
        char *data = job->data;
        size_t size = job->size;
        if (job->deflate) data = deflate_steps(job->data, job->size, &size, naps);
        if (data && ((mc && mc_write(path, data, size)) || save_write(path, data, size)))
            printf("SAVE wrote %s %d bytes in %d ms (queued %d ms)\n", path, (int)size, (int)((love_timer_getTime() - from) * 1000), (int)((from - job->at) * 1000));
        else job->s->failed = 1, printf("SAVE failed to write %s\n", path); // (saved() takes it from there)
        if (job->deflate) free(data);
    }
    free(job->data);
    free(job);
}
static void card_thread(void *arg) {
    for (Job *job;;) {
        WaitSema(card_wake);
        do { // (a job at a time under the lock: the game's side gets its turn between two)
            WaitSema(card_lock);
            if ((job = card_next())) card_do(job, 1);
            SignalSema(card_lock);
        } while (job);
    }
}
// At exit (love.event.quit, an ERROR): what is still waiting is written before the game ends
static void card_flush(void) {
    WaitSema(card_lock);
    for (Job *job; (job = card_next());) card_do(job, 0);
    SignalSema(card_lock);
}
extern void *_gp;
static void card_start(void) {
    static char stack[32 * 1024] __attribute__((aligned(16)));
    ee_sema_t sema = {.init_count = 1, .max_count = 1};
    card_lock = CreateSema(&sema);
    sema.init_count = 0;
    card_wake = CreateSema(&sema);
    int type, room, format; // (the card is asked for once before anything else: the server wants to have seen it)
    mc = !strncmp(base, "mc", 2) && mcInit(MC_TYPE_XMC) >= 0 && mc_wait(mcGetInfo(base[2] - '0', 0, &type, &room, &format)) >= -2;
    printf("SAVE the card's thread, %s\n", mc ? "libmc" : "NO libmc: writes through the files' server");
    // between the presenter (love_gs.c, 50) and the game (64)
    ee_thread_t thread = {.func = card_thread, .stack = stack, .stack_size = sizeof stack, .gp_reg = &_gp, .initial_priority = 60};
    StartThread(CreateThread(&thread), NULL);
    atexit(card_flush);
}
#endif

// Every file of the saves' directory and of the directories in it (the profiles') is read at the start: after
// that the card is only written, and a frame never waits for it to say that a file is not there.
static void save_list(const char *dir) {
    char names[32][32];
    int n = 0;
    DIR *d = opendir(*dir ? full(dir) : base);
    if (!d) return; // (then each save is looked up when it is first asked for)
    for (struct dirent *e; n < 32 && (e = readdir(d));)
        if (e->d_name[0] != '.' && strlen(e->d_name) < 32) strcpy(names[n++], e->d_name);
    closedir(d); // (before the files are opened: the card's module has few descriptors)
    for (int i = 0; i < n; i++) {
        char path[80];
        snprintf(path, sizeof path, *dir ? "%s/%s" : "%s%s", dir, names[i]);
        if (strchr(names[i], '.')) saved(path);
        else if (!*dir) entry(path)->known = entry(path)->dir = 1, save_list(path);
    }
    if (!*dir) listed = 1;
}

static Saved *entry(const char *path) {
    Saved *s;
    for (s = saves; s && strcmp(s->path, path); s = s->next) {}
    if (!s) {
        if (!(s = calloc(1, sizeof *s)) || !(s->path = strdup(path))) love_error("save: no memory");
        s->next = saves, saves = s, s->known = listed;
    }
    return s;
}
// The save of that path with its contents looked up (known 1: data is them, or NULL for no file)
static Saved *saved(const char *path) {
    Saved *s = entry(path);
#ifdef _EE
    if (s->failed) {
        s->failed = 0;
        if (!card_waiting(s)) free(s->data), s->data = NULL, s->known = 0; // whatever the card holds now
    }
    if (!s->known && card_lock >= 0) {
        WaitSema(card_lock); // (only after a failed write, or with no list of the card: it waits for the write going on)
        s->data = save_read(path, &s->size), s->known = 1;
        SignalSema(card_lock);
    }
#endif
    if (!s->known) s->data = card ? save_read(path, &s->size) : NULL, s->known = 1;
    return s;
}
// a then b (either may be the save's own data). deflate (EE): the card gets them deflated, the game reads them as they are
static bool store(const char *path, const void *a, size_t na, const void *b, size_t nb, int deflate) {
    char *data = malloc(na + nb + 1);
    if (!data) love_error("save: no memory");
    memcpy(data, a, na);
    memcpy(data + na, b, nb);
    data[na + nb] = 0;
    Saved *s = saved(path);
    free(s->data);
    s->data = data, s->size = na + nb;
#ifdef _EE
    if (card) card_queue(s, 'w', data, s->size, deflate); // a failure is known later: s->failed
#else
    if (card && !save_write(path, data, s->size)) {
        free(s->data);
        s->data = NULL, s->known = 0; // whatever the card holds now
        printf("SAVE failed to write %s\n", path);
        return false;
    }
#endif
    return true;
}

void *love_filesystem_read(const char *path, size_t *size) {
    Saved *s = saved(path);
    long n = s->data ? (long)s->size : game_file(path);
    char *data = n >= 0 ? malloc(n + 1) : NULL;
    if (!data) return NULL;
    if (s->data) memcpy(data, s->data, n);
    else if (!pack_read(love_game_path(path), 0, data, n)) return free(data), NULL;
    data[n] = 0;
    if (size) *size = n;
    return data;
}
bool love_filesystem_write(const char *path, const void *data, size_t size) { return store(path, data, size, "", 0, 0); }
bool love_filesystem_write_deflated(const char *path, const void *data, size_t size) {
#ifdef _EE
    return store(path, data, size, "", 0, 1);
#else
    size_t packed;
    void *deflated = love_data_compress(data, size, 1, &packed);
    bool ok = store(path, deflated, packed, "", 0, 0);
    free(deflated);
    return ok;
#endif
}
bool love_filesystem_append(const char *path, const void *data, size_t size) {
    Saved *s = saved(path);
    return store(path, s->data ? s->data : "", s->data ? s->size : 0, data, size, 0);
}
bool love_filesystem_remove(const char *path) {
    Saved *s = saved(path);
    free(s->data);
    s->data = NULL, s->known = 1, s->dir = 0;
    if (!card) return true;
#ifdef _EE
    card_queue(s, 'r', NULL, 0, 0);
#else
    save_remove(path);
#endif
    return true;
}
bool love_filesystem_createDirectory(const char *path) {
    Saved *s = saved(path);
    if (!card || s->dir) return true; // made once: the game asks before every save (save_manager.lua:21)
    s->dir = 1;
#ifdef _EE
    card_queue(s, 'd', NULL, 0, 0);
#else
    mkdir(full(path), 0777);
#endif
    return true;
}
bool love_filesystem_getInfo(const char *path, LoveFileInfo *info) {
    LoveFileInfo none;
    Saved *s = saved(path);
    long size = s->data ? (long)s->size : game_file(path);
    if (!info) info = &none;
    if (size >= 0) return *info = (LoveFileInfo){"file", size}, true;
    if (game_dir_has(path)) return *info = (LoveFileInfo){"directory", 0}, true;
    return false;
}
static int by_string(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
// The names are kept for good: a directory is asked for a few times in the whole game (the sounds, the shaders)
const char **love_filesystem_getDirectoryItems(const char *path, int *n) {
    size_t len = strlen(path);
    const char **items = calloc(nfiles + 1, sizeof *items);
    if (!items) love_error("no memory for a directory's list");
    *n = 0;
    for (int i = 0; i < nfiles; i++) {
        const char *rest = files[i].path;
        if (len && (strncmp(rest, path, len) || rest[len] != '/')) continue;
        if (len) rest += len + 1;
        size_t part = strcspn(rest, "/");
        int seen = 0;
        for (int j = 0; j < *n && !seen; j++) seen = !strncmp(items[j], rest, part) && !items[j][part];
        if (seen) continue;
        char *name = malloc(part + 1);
        if (!name) love_error("no memory for a directory's list");
        memcpy(name, rest, part);
        name[part] = 0;
        items[(*n)++] = name;
    }
    qsort(items, *n, sizeof *items, by_string);
    return items;
}

// A file of the game's directory or pack that the game's own list (files.txt) does not have: the port's data files.
void *love_game_read(const char *path, size_t *size) {
    long n = pack_size(love_game_path(path));
    char *data = n >= 0 ? malloc(n + 1) : NULL;
    if (!data) return NULL;
    if (!pack_read(love_game_path(path), 0, data, n)) return free(data), NULL;
    data[n] = 0;
    if (size) *size = n;
    return data;
}
