// The GS backend of gfx.c (gfx_gs.c, EE only).
struct GsTex;
// A shader's recipe for one texel (i, j) of the sprite being drawn, counted from its corner (gfx.c): it changes
// rgba, and may read the sprite's other texels as the atlas has them through fetch.
typedef void Fetch(int i, int j, unsigned char *rgba);
typedef void Filter(int i, int j, unsigned char *rgba, Fetch *fetch);
void gs_init(void);
// A .tex file of w x h texels whose palette is pal; its texels are read when they are first drawn.
struct GsTex *gs_texture(int w, int h, const char *path, const unsigned char pal[256][4]);
// As raster() of gfx.c: p0, p0 + e1, p0 + e2 (and their sum unless tri), flat (t NULL) or textured, times colour.
// filter, if any, changes each texel drawn, the texel at (u, v) being its (0, 0). What it makes of these texels
// is kept under kind (not 0) and made again only for another stamp, and then not every frame; stamp 0: never kept.
void gs_raster(const float p0[2], const float e1[2], const float e2[2], const float bend[2], int tri, struct GsTex *t, float u, float v, float du,
               float dv, const float colour[4], Filter *filter, int kind, unsigned stamp);
// The same of a w x h picture of 8-bit RGBA made this frame, sampled between its texels if linear.
void gs_raster_rgba(const float p0[2], const float e1[2], const float e2[2], const unsigned char *rgba, int w, int h, float u,
                    float v, float du, float dv, int linear);
void gs_smooth(struct GsTex *t); // sampled between its texels from now on
// Picture id (0 to 2) of a swirl over the whole screen: w x h palette indices, read again if fresh
void gs_swirl(int id, const unsigned char *idx, int w, int h, int fresh, const unsigned char pal[256][4], float alpha);
void gs_clear(const float colour[4]);
void gs_present(void);
