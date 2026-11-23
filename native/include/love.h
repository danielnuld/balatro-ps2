// The platform of the native port: what the game takes from LÖVE 11, as C. See docs/conventions.md ("The platform").
// love.graphics.setColor(r, g, b, a) is love_graphics_setColor(r, g, b, a); a method of a LÖVE object,
// image:getWidth(), is Image_getWidth(image). Only what the game uses exists.
//
// Two targets: the PS2 (the GS through gsKit, audsrv, libpad, the memory card) and the Linux host, where a
// software rasteriser draws the same picture for tests and screenshots.
// Implemented in src/love_gfx.c (and love_gs.c on the PS2), love_snd.c, love_pad.c, love_fs.c, love_misc.c.
#ifndef LOVE_H
#define LOVE_H
#include "core.h"

#define LOVE_SCREEN_W 640 // the PS2's picture: NTSC, interlaced
#define LOVE_SCREEN_H 448

// ---------------------------------------------------------------- boot and the frame
// What boot.lua took from its arguments and the environment. love_init(NULL) takes the defaults: on the host
// GAME_DIR (build/game), PACK, SAVE_DIR, DT and SHOT of the environment; on the PS2 host:game, or the disc's
// pack with -DDISC, and the memory card with -DPLAY.
typedef struct LoveConf {
    const char *game;     // the directory of the game's files, or "pak:" for the mounted pack
    const char *pack;     // the pack file to mount (tools/disc.py), or NULL
    const char *save_dir; // where the game's saves go (mc0:/BALATRO); NULL: they stay in memory
    bool soft;            // host: the software rasteriser draws (screenshots); else nothing is drawn there
    double dt;            // the fixed step love_timer_step returns; 0: the measured one
} LoveConf;
void love_init(const LoveConf *conf); // everything below is ready after it: the pack, the screen, the font, the pad, the sound, the saves
bool love_frame_begin(void);          // counts a frame (love_frame); false once love_event_quit was called
void love_present(void);              // the end of a frame: love_graphics_present, then the sound's share (love_audio_pump)
int love_frame(void);                 // frames begun so far
void love_error(const char *format, ...) __attribute__((noreturn, format(printf, 1, 2))); // prints "ERROR ..." and exits

// ---------------------------------------------------------------- love.graphics
// Geometry is float. A texture is an atlas of tools/textures.py (<png>.tex: 8-bit texels and a palette).
typedef struct Image { int id; float w, h, k; } Image; // w, h in LÖVE's units; k texels per unit (0.5: a 2x image at 1x)
typedef Image Canvas;                                  // the one canvas the game draws to is the screen
typedef struct Quad { float x, y, w, h; } Quad;        // a rectangle of an image, in its units
typedef struct Font { float k; } Font;                 // the one bitmap font (tools/font.py) at k times its metrics' size
typedef struct Text { // (after text: love_gfx.c's own, where it was drawn last, to tell a text at rest from one that moves)
    Font *font; char *text;
    float at[2][3]; int frame; unsigned char draws, known, moved[2];
} Text;
typedef struct Shader Shader;                          // a shader file's name and what was sent to it

static inline bool love_graphics_isActive(void) { return true; }  // love.graphics.isActive()
static inline bool love_graphics_isCreated(void) { return true; } // love.graphics.isCreated()
int love_graphics_getWidth(void);                                 // the screen's width in pixels (640)
int love_graphics_getHeight(void);                                // the screen's height in pixels (448)
void love_graphics_getDimensions(int *w, int *h);                 // both
// love.graphics.newImage(path, {dpiscale = n}): always the 1x atlas ("/2x/" in the path reads "/1x/"); dpiscale 0: not given
Image *love_graphics_newImage(const char *path, float dpiscale);
static inline float Image_getWidth(const Image *self) { return self->w; }       // image:getWidth(), also a canvas's
static inline float Image_getHeight(const Image *self) { return self->h; }      // image:getHeight()
static inline void Image_getDimensions(const Image *self, float *w, float *h) { *w = self->w, *h = self->h; } // image:getDimensions()
static inline float Image_getPixelWidth(const Image *self) { return self->w; }  // image:getPixelWidth() (as boot.lua: the units)
static inline float Image_getPixelHeight(const Image *self) { return self->h; } // image:getPixelHeight()
static inline void Image_setFilter(Image *self, const char *min, const char *mag) {} // image:setFilter(): nearest always (the font: between texels)
Canvas *love_graphics_newCanvas(float w, float h);                // love.graphics.newCanvas(w, h); 0: the screen's size
void love_graphics_setCanvas(Canvas *canvas);                     // draw to that canvas; NULL: to the screen
Quad *love_graphics_newQuad(float x, float y, float w, float h, float sw, float sh); // love.graphics.newQuad (sw, sh unused)
static inline void Quad_setViewport(Quad *self, float x, float y, float w, float h) { self->x = x, self->y = y, self->w = w, self->h = h; } // quad:setViewport
Font *love_graphics_newFont(const char *file, float size);        // love.graphics.newFont(file, size): the one font, at that size (0: 12)
Font *love_graphics_setNewFont(const char *file, float size);     // newFont, and it becomes the font of print
float Font_getWidth(const Font *self, const char *text);          // font:getWidth(text): the sum of its advances
float Font_getHeight(const Font *self);                           // font:getHeight()
bool Font_hasGlyphs(const Font *self, const char *text);          // font:hasGlyphs(text) ('?' is drawn for the others)
static inline void Font_setFilter(Font *self, const char *min, const char *mag) {} // font:setFilter(): nothing to choose
Text *love_graphics_newText(Font *font, const char *text);        // love.graphics.newText(font, text); a coloured text's colours are dropped: pass its string
void Text_set(Text *self, const char *text);                      // text:set(string)
float Text_getWidth(const Text *self);                            // text:getWidth()
float Text_getHeight(const Text *self);                           // text:getHeight()
void Text_getDimensions(const Text *self, float *w, float *h);    // text:getDimensions()
void Text_release(Text *self);                                    // frees it now (optional: the collector finds a Text nothing refers to)
void love_graphics_print(const char *text, float x, float y, float r, float sx, float sy); // love.graphics.print with the current font
void love_graphics_printf(const char *text, Font *font, float x, float y); // love.graphics.printf: no wrap, no align; font NULL: the current one
// love.graphics.draw: of a whole image or canvas, of a quad of an image, of a Text. sx, sy: pass 1 where the Lua gives none.
void love_graphics_draw(Image *image, float x, float y, float r, float sx, float sy, float ox, float oy);
void love_graphics_draw_quad(Image *image, const Quad *quad, float x, float y, float r, float sx, float sy, float ox, float oy);
void love_graphics_draw_text(const Text *text, float x, float y, float r, float sx, float sy, float ox, float oy);
void love_graphics_push(void);                                    // love.graphics.push(): 64 deep
void love_graphics_pop(void);                                     // love.graphics.pop()
void love_graphics_origin(void);                                  // love.graphics.origin()
void love_graphics_translate(float x, float y);                   // love.graphics.translate(x, y)
void love_graphics_rotate(float r);                               // love.graphics.rotate(r)
void love_graphics_scale(float sx, float sy);                     // love.graphics.scale(sx, sy)
void love_graphics_setColor(float r, float g, float b, float a);  // love.graphics.setColor(r, g, b, a)
static inline void love_graphics_setColorv(const float *rgba) { love_graphics_setColor(rgba[0], rgba[1], rgba[2], rgba[3]); } // setColor({r, g, b, a})
void love_graphics_setLineWidth(float width);                     // love.graphics.setLineWidth(w)
static inline void love_graphics_setLineStyle(const char *style) {}                        // love.graphics.setLineStyle: lines are plain rectangles
static inline void love_graphics_setDefaultFilter(const char *min, const char *mag) {}     // love.graphics.setDefaultFilter: as Image_setFilter
void love_graphics_clear(float r, float g, float b, float a);     // love.graphics.clear(r, g, b, a); clear() is (0, 0, 0, 0)
void love_graphics_rectangle(const char *mode, float x, float y, float w, float h); // "fill" or "line"; no corner radius
void love_graphics_polygon(const char *mode, const float *xy, int n); // n points (x, y, x, y...): a convex one filled as a fan, or its outline
void love_graphics_reset(void);                                   // love.graphics.reset(): origin, white, the screen
// A shader is its file's name and what was sent to it; love_gfx.c has a recipe for the names it knows ("shaders"
// there) and the rest pass through. The uniforms are read when the shader is set, as boot.lua does: send first.
Shader *love_graphics_newShader(const char *path);                // love.graphics.newShader("resources/shaders/x.fs")
void Shader_send(Shader *self, const char *name, float value);    // shader:send(name, number); a boolean as 1 or 0
void Shader_send_vec(Shader *self, const char *name, const float *v, int n); // shader:send(name, {a, b...}): n of 2 to 4
static inline void Shader_send_colour(Shader *self, const char *name, const float *rgba) { Shader_send_vec(self, name, rgba, 4); } // shader:send(name, colour)
void love_graphics_setShader(Shader *shader);                     // love.graphics.setShader(shader); NULL: none
void love_graphics_present(void);                                 // love.graphics.present(): the frame is done
int love_graphics_count(void);                                    // primitives drawn since the last call (PERF lines)
bool love_graphics_shot(const char *file);                        // host: the screen as a PPM; false if it cannot (no rasteriser)

// ---------------------------------------------------------------- love.timer
double love_timer_getTime(void);     // love.timer.getTime(): seconds, the real clock
double love_timer_step(void);        // love.timer.step(): the frame's dt, measured or the fixed one of LoveConf
double love_timer_getDelta(void);    // love.timer.getDelta(): the last step's dt
int love_timer_getFPS(void);         // love.timer.getFPS(), from the last dt, 60 at most
static inline void love_timer_sleep(float seconds) {} // love.timer.sleep: the frame is paced by the vertical blank of present

// ---------------------------------------------------------------- love.filesystem and love.data
// A path is the game's ("resources/sounds/x.ogg") or one of its saves ("1/profile.jkr"): a save hides a game file.
typedef struct LoveFileInfo { const char *type; long size; } LoveFileInfo; // type: "file" or "directory"
void *love_filesystem_read(const char *path, size_t *size);       // the contents (a 0 after them), to free(); NULL: no such file
void *love_game_read(const char *path, size_t *size);             // the same for a file that is not in the game's list (the data files on a disc)
bool love_filesystem_write(const char *path, const void *data, size_t size); // a save; false if the card did not take it
bool love_filesystem_write_deflated(const char *path, const void *data, size_t size); // write(path, compress(data, 1)), the deflate left to the card's thread (EE): a read before the next boot may give data as it is
bool love_filesystem_append(const char *path, const void *data, size_t size); // a save, after what it holds
bool love_filesystem_remove(const char *path);                    // love.filesystem.remove(path) of a save
bool love_filesystem_createDirectory(const char *path);           // love.filesystem.createDirectory(path) in the saves
bool love_filesystem_getInfo(const char *path, LoveFileInfo *info); // love.filesystem.getInfo: false is nil; info may be NULL
const char **love_filesystem_getDirectoryItems(const char *path, int *n); // the game's files and directories there, sorted; to free() (not the names)
const char *love_filesystem_getSourceBaseDirectory(void);         // the game's directory
void *love_data_compress(const void *data, size_t size, int level, size_t *out_size);   // love.data.compress("string", "deflate", data, level): raw deflate, to free(); level -1: zlib's default
void *love_data_decompress(const void *data, size_t size, size_t *out_size); // love.data.decompress("string", "deflate", data), to free(); NULL: not deflate data

// ---------------------------------------------------------------- love.audio
// A sound is <ogg>.snd of tools/sounds.py (SPU ADPCM, mono, 22050 Hz), mixed in software, 24 voices.
typedef struct Source Source;
Source *love_audio_newSource(const char *path, const char *kind); // love.audio.newSource(path, "static" | "stream"); long sounds stream anyway
bool Source_play(Source *self);                // source:play(); false when every voice is busy
void Source_stop(Source *self);                // source:stop()
bool Source_isPlaying(const Source *self);     // source:isPlaying()
void Source_setVolume(Source *self, float v);  // source:setVolume(v), clamped to 0..1
void Source_setPitch(Source *self, float v);   // source:setPitch(v)
void Source_release(Source *self);             // source:release(): stopped and freed
static inline bool love_audio_play(Source *source) { return Source_play(source); } // love.audio.play(source)
void love_audio_stop(void);                    // love.audio.stop(): every source
int love_audio_pump(float dt);                 // once a frame (love_present does): streams topped up; host: dt of sound mixed. The voices playing

// ---------------------------------------------------------------- love.joystick and love.event
// The DualShock 2 is LÖVE's one gamepad. Buttons and axes have SDL's names: "a" (cross), "b" (circle), "x"
// (square), "y" (triangle), "dpup", "dpdown", "dpleft", "dpright", "leftshoulder", "rightshoulder", "start",
// "guide" (SELECT), "leftstick", "rightstick"; axes "leftx", "lefty", "rightx", "righty", "triggerleft",
// "triggerright" (L2, R2: 0 or 1).
typedef struct Joystick Joystick;
Joystick **love_joystick_getJoysticks(int *n);                    // love.joystick.getJoysticks(): the one pad
static inline void love_joystick_loadGamepadMappings(const char *file) {} // nothing to map
static inline bool Joystick_isGamepad(const Joystick *self) { return true; } // joystick:isGamepad()
static inline const char *Joystick_getGamepadMappingString(const Joystick *self) { return "0,PS2 Controller,"; } // the game shows PlayStation glyphs for "PS<digit>"
float Joystick_getGamepadAxis(const Joystick *self, const char *axis);   // joystick:getGamepadAxis(name): -1..1, as of the last pump
bool Joystick_isGamepadDown(const Joystick *self, const char *button);   // the button as of the last pump
static inline void Joystick_setVibration(Joystick *self, float left, float right) {} // ponytail: no rumble; padSetActDirect if it is missed
void love_joystick_press(const char *button);                     // tests and bots: the button is down for the next two frames
typedef enum { LOVE_GAMEPADPRESSED, LOVE_GAMEPADRELEASED, LOVE_JOYSTICKAXIS } LoveEventType;
typedef struct LoveEvent {
    LoveEventType type;
    Joystick *joystick;
    const char *button; // gamepadpressed, gamepadreleased
    int axis;           // joystickaxis: 1 a stick moved, 5 a trigger is down
    float value;
} LoveEvent;
void love_event_pump(void);            // love.event.pump(): reads the pad (with what was tapped at any vertical blank since) and queues what changed
bool love_event_poll(LoveEvent *e);    // the next queued event; false: none left
void love_event_quit(void);            // love.event.quit(): love_frame_begin returns false from now on

// ---------------------------------------------------------------- love.system, love.window, love.mouse, love.touch
static inline const char *love_system_getOS(void) { return "ps4"; }           // the game's own PlayStation branch (globals.lua)
static inline const char *love_system_getClipboardText(void) { return ""; }   // no clipboard
static inline void love_system_setClipboardText(const char *text) {}          // no clipboard
static inline void love_system_openURL(const char *url) {}                    // no browser
static inline bool love_window_setMode(int w, int h) { return true; }         // love.window.setMode: the mode is the PS2's
static inline bool love_window_updateMode(int w, int h) { return true; }      // love.window.updateMode
static inline void love_window_getMode(int *w, int *h) { *w = LOVE_SCREEN_W, *h = LOVE_SCREEN_H; } // flags: fullscreen, vsync 1, display 1
static inline void love_window_getDesktopDimensions(int *w, int *h) { *w = LOVE_SCREEN_W, *h = LOVE_SCREEN_H; } // love.window.getDesktopDimensions
static inline int love_window_getDisplayCount(void) { return 1; }             // love.window.getDisplayCount
static inline const char *love_window_getDisplayName(void) { return "PlayStation 2"; } // love.window.getDisplayName
static inline int love_window_getFullscreenModes(int *w, int *h) { *w = LOVE_SCREEN_W, *h = LOVE_SCREEN_H; return 1; } // the one mode; returns their count
static inline const char *love_window_getTitle(void) { return "Balatro"; }    // love.window.getTitle
static inline bool love_window_isOpen(void) { return true; }                  // love.window.isOpen
static inline float love_window_toPixels(float x) { return x; }               // love.window.toPixels
static inline void love_mouse_getPosition(float *x, float *y) { *x = *y = 0; } // no mouse
static inline bool love_mouse_isVisible(void) { return false; }               // love.mouse.isVisible
static inline void love_mouse_setVisible(bool visible) {}                     // love.mouse.setVisible
static inline void love_mouse_setGrabbed(bool grabbed) {}                     // love.mouse.setGrabbed
static inline void love_mouse_setRelativeMode(bool relative) {}               // love.mouse.setRelativeMode
static inline int love_touch_getTouches(void) { return 0; }                   // love.touch.getTouches(): none (their count)

// ---------------------------------------------------------------- love.thread
// The game's sound and save managers are LÖVE threads that loop on channel:demand(). Here the loop's body is a
// function, given once with love_thread_serve in the place of newThread(file):start(), and called inside every
// push to its channel: a request is served before the push returns (boot.lua did the same with a coroutine).
// A pushed table is copied (tcopy), as between LÖVE's threads, except on "save_request", whose tables are used
// before the push returns. Channels nobody serves or pops ("http_request"...) drop what is pushed.
typedef struct Channel Channel;
typedef void (*ChannelFn)(Channel *channel);
Channel *love_thread_getChannel(const char *name);          // love.thread.getChannel(name)
void love_thread_serve(const char *name, ChannelFn serve);  // the thread whose loop demands from that channel
void Channel_push(Channel *self, Val v);                    // channel:push(v)
Val Channel_pop(Channel *self);                             // channel:pop(): NIL when empty
Val Channel_demand(Channel *self);                          // channel:demand() inside the serving function: the request it was called for

// ---------------------------------------------------------------- math.random
// Lua's math.random and math.randomseed as LuaJIT 2.0 has them, the generator the game's seeded runs depend on.
void math_randomseed(double seed);         // math.randomseed(seed)
double math_random(void);                  // math.random(): [0, 1)
double math_random_to(double m);           // math.random(m): an integer 1..m
double math_random_range(double lo, double hi); // math.random(lo, hi): an integer lo..hi

// ---------------------------------------------------------------- inside the platform (not for the game's code)
#ifdef LOVE_INTERNAL
// The game's files: a directory, or the pack of a disc (love_fs.c).
int pack_mount(const char *path);  // the pack file; 0 if it is not one
long pack_size(const char *path);  // a game file's length, -1 if there is none
// n bytes of a game file from offset, in one request. 0: no such file, or it is shorter.
int pack_read(const char *path, long offset, void *to, long n);
const char *love_game_path(const char *path); // the game's directory + "/" + path, in a buffer of its own until the next call (main thread only)
void love_fs_init(const char *game, const char *save_dir); // the game's file list; the saves' directory (after the pad: its modules are the card's too)
void love_gfx_init(int w, int h, bool soft);  // the screen and the font
void love_snd_init(void);
void love_pad_init(void);
unsigned love_pad_read(float axes[4]);        // the 16 buttons, 1 = pressed, in libpad's bit order; the sticks, each axis -1..1

// The GS backend of love_gfx.c (love_gs.c, EE only).
struct GsTex;
// A shader's recipe for one texel (i, j) of the sprite being drawn, counted from its corner (love_gfx.c): it
// changes rgba, and may read the sprite's other texels as the atlas has them through fetch.
typedef void Fetch(int i, int j, unsigned char *rgba);
typedef void Filter(int i, int j, unsigned char *rgba, Fetch *fetch);
void gs_init(void);
// A .tex file of w x h texels whose palette is pal; its texels are read when they are first drawn.
struct GsTex *gs_texture(int w, int h, const char *path, const unsigned char pal[256][4]);
// As raster() of love_gfx.c: p0, p0 + e1, p0 + e2 (and their sum unless tri), flat (t NULL) or textured, times colour.
// filter, if any, changes each texel drawn, the texel at (u, v) being its (0, 0). What it makes of these texels
// is kept under kind (not 0) and made again only for another stamp, and then not every frame; stamp 0: never kept.
void gs_raster(const float p0[2], const float e1[2], const float e2[2], const float bend[2], int tri, struct GsTex *t, float u, float v, float du,
               float dv, const float colour[4], Filter *filter, int kind, unsigned stamp);
// The same of a w x h picture of 8-bit RGBA made this frame, sampled between its texels if linear.
void gs_raster_rgba(const float p0[2], const float e1[2], const float e2[2], const unsigned char *rgba, int w, int h, float u,
                    float v, float du, float dv, int linear);
// A filled polygon of n corners in target pixels (3 to 256), flat
void gs_fan(const float (*corners)[2], int n, const float colour[4]);
void gs_smooth(struct GsTex *t); // sampled between its texels from now on
// Picture id (0 to 2) of a swirl over the whole screen: w x h palette indices, read again if fresh
void gs_swirl(int id, const unsigned char *idx, int w, int h, int fresh, const unsigned char pal[256][4], float alpha);
void gs_clear(const float colour[4]);
void gs_present(void);
int gs_keeping_up(void); // whether the game's frames have been fitting in a vertical blank
// The picture's modes (love_gs.c's table): 0 is 480i, the one every cable shows. gs_mode sets one (before the first
// gs_present only): the game's 640x448 is to be stretched over its w x h pixels (love_gfx_scale).
const char *gs_mode_name(int mode); // NULL beyond the last
int gs_mode(int mode, int *w, int *h); // 0: the screen gives no vertical blank in that mode, and 480i is set again
int gs_forced(void);                  // a loader decides the video mode (asked in 480i): no other mode can be set
void gs_show(void);              // what was drawn since the last one, on screen at the next vertical blank (before the first gs_present only)
void love_gfx_scale(int w, int h); // love_gfx.c: the pixels of the picture the game's 640x448 is drawn over, and the font's levels for them
unsigned love_pad_peek(void);    // the 16 buttons as love_pad_read gives them, with nothing started for it (before the game's first frame)
void love_video_pick(void);      // love_video.c: the screen that asks for the picture's mode, before the game (EE)
#endif
#endif
