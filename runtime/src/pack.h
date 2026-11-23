// The game's files: a directory, or the pack of a disc (pack.c).
int pack_mount(const char *path);  // the pack file; 0 if it is not one
long pack_size(const char *path);  // a game file's length, -1 if there is none
// n bytes of a game file from offset, in one request. 0: no such file, or it is shorter.
int pack_read(const char *path, long offset, void *to, long n);
