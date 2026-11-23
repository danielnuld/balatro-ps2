// The platform layer on the host: files, saves, deflate, the pad's events, the mixer, the random numbers, and a
// picture drawn by the software rasteriser (sprites of an atlas, text, polygons, shaders' recipes) written as a PPM.
// usage: love_test [game_dir] [out.ppm]      (PACK=<file> in the environment and "pak:" as game_dir: from a pack;
//                                             SAVE_DIR=<directory>: the saves go through the card's file format)
// Build (from native/): gcc -std=gnu99 -Wall -g -fsanitize=address,undefined -Iinclude src/love_gfx.c src/love_snd.c
//   src/love_pad.c src/love_fs.c src/love_misc.c src/gc.c test/love_test.c -lz -lm -o love_test
#include <assert.h>

#include "love.h"

#ifndef WITH_CORE // core.c is not linked: the three things of it love_misc.c's channels touch
const Val NIL = {V_NIL};
Val T(Table *t) { Val v = {V_TAB}; v.tab = t; return v; }
Table *tcopy(Table *t) { return t; }
#endif

#define CHECK(what) do { if (!(what)) { printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #what); exit(1); } } while (0)

static int served;
static void serve(Channel *channel) {
    Val request = Channel_demand(channel);
    CHECK(request.t == V_NUM);
    served += (int)request.n;
}

static void test_random(void) { // the numbers the game's own lua51.dll gives for these seeds (boot.lua)
    math_randomseed(0.5);
    CHECK(math_random() == 0.09657393438653461 && math_random() == 0.96226945770684003);
    CHECK(math_random_to(52) == 28 && math_random_range(10, 20) == 11);
    math_randomseed(1700000000);
    CHECK(math_random() == 0.66970184905361352);
}

static void test_files(void) {
    size_t size = 0, packed = 0, back = 0;
    LoveFileInfo info;
    char *version = love_filesystem_read("version.jkr", &size);
    CHECK(version && size > 0 && strlen(version) == size);
    printf("version.jkr: %.*s\n", (int)strcspn(version, "\r\n"), version);
    free(version);
    CHECK(love_filesystem_getInfo("main.lua", &info) && !strcmp(info.type, "file") && info.size > 0);
    CHECK(love_filesystem_getInfo("resources/sounds", &info) && !strcmp(info.type, "directory"));
    CHECK(!love_filesystem_getInfo("no/such/file", NULL) && !love_filesystem_read("no/such/file", NULL));
    int n = 0, found = 0;
    const char **items = love_filesystem_getDirectoryItems("resources/shaders", &n);
    for (int i = 0; i < n; i++) found += !strcmp(items[i], "background.fs") || !strcmp(items[i], "dissolve.fs");
    for (int i = 1; i < n; i++) CHECK(strcmp(items[i - 1], items[i]) < 0);
    CHECK(found == 2);
    free(items);
    items = love_filesystem_getDirectoryItems("", &n);
    for (int i = found = 0; i < n; i++) found += !strcmp(items[i], "resources") || !strcmp(items[i], "main.lua");
    CHECK(found == 2);
    free(items);

    // a save, as the game's string packer writes one: deflated, then read back
    static const char text[] = "return {[\"GAMESPEED\"] = 2, [\"tutorial_complete\"] = true, [\"language\"] = \"en-us\", [\"language2\"] = \"en-us\"}";
    char *deflated = love_data_compress(text, strlen(text), 1, &packed);
    CHECK(packed > 0 && packed < strlen(text));
    love_filesystem_createDirectory("1");
    love_filesystem_remove("1/test.jkr");
    CHECK(!love_filesystem_getInfo("1/test.jkr", NULL));
    CHECK(love_filesystem_write("1/test.jkr", deflated, packed));
    CHECK(love_filesystem_getInfo("1/test.jkr", &info) && info.size == (long)packed);
    char *read = love_filesystem_read("1/test.jkr", &size);
    CHECK(read && size == packed && !memcmp(read, deflated, packed));
    char *inflated = love_data_decompress(read, size, &back);
    CHECK(inflated && back == strlen(text) && !strcmp(inflated, text));
    CHECK(!love_data_decompress(text, strlen(text), &back)); // not deflate data
    free(deflated), free(read), free(inflated);
    CHECK(love_filesystem_append("1/test.jkr", "ab", 2) && love_filesystem_append("1/test.jkr", "cd", 2));
    read = love_filesystem_read("1/test.jkr", &size);
    CHECK(size == packed + 4 && !memcmp(read + packed, "abcd", 4));
    free(read);
    CHECK(love_filesystem_remove("1/test.jkr") && !love_filesystem_getInfo("1/test.jkr", NULL));
    // (left behind for a second run to find, with SAVE_DIR: what the card holds comes back)
    char *kept = love_filesystem_read("1/kept.jkr", &size);
    printf("saves: 1/kept.jkr %s\n", kept ? kept : "not there yet");
    CHECK(!kept || !strcmp(kept, "kept"));
    free(kept);
    CHECK(love_filesystem_write("1/kept.jkr", "kept", 4));
}

static void test_pad(void) {
    LoveEvent e;
    int n = 0;
    Joystick *pad = love_joystick_getJoysticks(&n)[0];
    CHECK(n == 1 && Joystick_isGamepad(pad) && strstr(Joystick_getGamepadMappingString(pad), "PS2"));
    love_event_pump();
    CHECK(!love_event_poll(&e));
    love_joystick_press("a");
    love_joystick_press("triggerleft");
    CHECK(love_frame_begin());
    love_event_pump();
    CHECK(love_event_poll(&e) && e.type == LOVE_JOYSTICKAXIS && e.axis == 5 && e.value == 1);
    CHECK(love_event_poll(&e) && e.type == LOVE_GAMEPADPRESSED && !strcmp(e.button, "a") && e.joystick == pad);
    CHECK(!love_event_poll(&e));
    CHECK(Joystick_isGamepadDown(pad, "a") && !Joystick_isGamepadDown(pad, "b") && Joystick_getGamepadAxis(pad, "triggerleft") == 1);
    CHECK(Joystick_getGamepadAxis(pad, "leftx") == 0);
    love_frame_begin(), love_frame_begin();
    love_event_pump();
    CHECK(love_event_poll(&e) && e.type == LOVE_GAMEPADRELEASED && !strcmp(e.button, "a") && !love_event_poll(&e));
    CHECK(!Joystick_isGamepadDown(pad, "a") && Joystick_getGamepadAxis(pad, "triggerleft") == 0);

    love_thread_serve("sound_request", serve);
    Channel_push(love_thread_getChannel("sound_request"), N(3));
    Channel_push(love_thread_getChannel("sound_request"), N(4));
    CHECK(served == 7 && Channel_pop(love_thread_getChannel("sound_request")).t == V_NIL);
    Channel_push(love_thread_getChannel("http_request"), N(1)); // nobody reads it: dropped
    CHECK(Channel_pop(love_thread_getChannel("http_request")).t == V_NIL);
    Channel_push(love_thread_getChannel("load_channel"), N(5));
    CHECK(Channel_pop(love_thread_getChannel("load_channel")).n == 5);
}

static void test_sound(void) {
    Source *chip = love_audio_newSource("resources/sounds/chips1.ogg", "static");
    Source *again = love_audio_newSource("resources/sounds/chips1.ogg", "static"); // the same sample, shared
    Source *music = love_audio_newSource("resources/sounds/music1.ogg", "stream");
    CHECK(!Source_isPlaying(chip));
    Source_setVolume(chip, 0.8f), Source_setPitch(chip, 1.2f);
    Source_setVolume(music, 3); // (clamped to 1)
    CHECK(love_audio_play(chip) && Source_play(again) && Source_play(music));
    CHECK(Source_isPlaying(chip) && Source_isPlaying(music));
    CHECK(love_audio_pump(1 / 60.0f) == 3);
    Source_setPitch(music, 0.7f);
    for (int i = 0; i < 600; i++) love_audio_pump(1 / 60.0f); // ten seconds: the effect is over, the music goes on
    CHECK(!Source_isPlaying(chip) && !Source_isPlaying(again) && Source_isPlaying(music));
    CHECK(love_audio_pump(0) == 1);
    Source_release(chip);
    love_audio_stop();
    CHECK(!Source_isPlaying(music) && love_audio_pump(0) == 0);
    Source_release(music), Source_release(again);
}

// The uniforms every card shader takes (engine/sprite.lua draw_shader)
static void card_uniforms(Shader *s, float dissolve, bool shadow) {
    static const float none[4] = {0, 0, 0, 0}, mouse[2] = {320, 224};
    Shader_send(s, "dissolve", dissolve);
    Shader_send(s, "time", 123.33f);
    Shader_send(s, "shadow", shadow);
    Shader_send_colour(s, "burn_colour_1", none);
    Shader_send_colour(s, "burn_colour_2", none);
    Shader_send_vec(s, "mouse_screen_pos", mouse, 2);
    Shader_send(s, "screen_scale", 1);
    Shader_send(s, "hovering", 0);
}

static void test_picture(const char *out) {
    int w, h;
    love_graphics_getDimensions(&w, &h);
    CHECK(w == LOVE_SCREEN_W && h == LOVE_SCREEN_H && love_graphics_getWidth() == 640 && love_graphics_getHeight() == 448);
    Image *deck = love_graphics_newImage("resources/textures/2x/8BitDeck.png", 2); // (the game asks for 2x: the 1x atlas, in 1x units)
    Image *jokers = love_graphics_newImage("resources/textures/1x/Jokers.png", 1);
    CHECK(Image_getWidth(deck) == 71 * 13 && Image_getHeight(deck) == 95 * 4);
    Canvas *canvas = love_graphics_newCanvas(0, 0);
    Shader *background = love_graphics_newShader("resources/shaders/background.fs");
    Shader *dissolve = love_graphics_newShader("resources/shaders/dissolve.fs"), *foil = love_graphics_newShader("resources/shaders/foil.fs");
    Shader *negative = love_graphics_newShader("resources/shaders/negative.fs"), *polychrome = love_graphics_newShader("resources/shaders/polychrome.fs");
    Font *font = love_graphics_newFont("resources/fonts/m6x11plus.ttf", 200); // as the game: big, drawn scaled down
    CHECK(Font_getHeight(font) == 200 && Font_getWidth(font, "A") > 0 && Font_getWidth(font, "AA") == 2 * Font_getWidth(font, "A"));
    CHECK(Font_hasGlyphs(font, "Mult x2") && !Font_hasGlyphs(font, "\xe4\xb8\xad"));
    Text *title = love_graphics_newText(font, "?");
    Text_set(title, "Balatro on the PS2: 300 X 4 = $1,200");
    float tw, th;
    Text_getDimensions(title, &tw, &th);
    CHECK(tw == Font_getWidth(font, "Balatro on the PS2: 300 X 4 = $1,200") && th == 200);

    love_graphics_setCanvas(canvas); // as the game: everything to its canvas, then the canvas to the screen
    love_graphics_clear(0, 0, 0, 1);
    love_graphics_setColor(1, 1, 1, 1);
    // the swirl behind a run (game.lua: the background sprite with its shader)
    static const float c1[4] = {0.996f, 0.373f, 0.333f, 1}, c2[4] = {0, 0.616f, 1, 1}, c3[4] = {0.216f, 0.259f, 0.267f, 1};
    Shader_send(background, "time", 12.5f);
    Shader_send(background, "spin_time", 12.5f);
    Shader_send_colour(background, "colour_1", c1);
    Shader_send_colour(background, "colour_2", c2);
    Shader_send_colour(background, "colour_3", c3);
    Shader_send(background, "contrast", 1.5f);
    Shader_send(background, "spin_amount", 0.3f);
    Quad *cell = love_graphics_newQuad(0, 0, 71, 95, 71 * 13, 95 * 4);
    Image *enhancers = love_graphics_newImage("resources/textures/1x/Enhancers.png", 1);
    Quad *base = love_graphics_newQuad(71, 0, 71, 95, Image_getWidth(enhancers), Image_getHeight(enhancers));
    love_graphics_setShader(background);
    love_graphics_draw_quad(deck, cell, 0, 0, 0, 1, 1, 0, 0);
    love_graphics_setShader(NULL);

    // a hand: five cards of the deck's atlas in a fan, each through dissolve.fs as the game draws them, its shadow first
    love_graphics_push();
    love_graphics_translate(320, 300);
    for (int i = 0; i < 5; i++) {
        Quad_setViewport(cell, 71 * (8 + i), 95 * (i % 4), 71, 95);
        for (int shadow = 1; shadow >= 0; shadow--) {
            card_uniforms(dissolve, i == 4 ? 0.45f : 0, shadow);
            if (i == 4) Shader_send_colour(dissolve, "burn_colour_1", c1), Shader_send_colour(dissolve, "burn_colour_2", c2);
            love_graphics_setShader(dissolve);
            love_graphics_push();
            love_graphics_translate((i - 2) * 80 + (shadow ? 4 : 0), fabsf(i - 2) * 8 + (shadow ? 6 : 0));
            love_graphics_rotate((i - 2) * 0.08f);
            love_graphics_scale(1.3f, 1.3f);
            love_graphics_draw_quad(enhancers, base, -35.5f, -47.5f, 0, 1, 1, 0, 0); // the card itself (c_base), then its face
            if (!shadow) love_graphics_draw_quad(deck, cell, -35.5f, -47.5f, 0, 1, 1, 0, 0);
            love_graphics_pop();
        }
    }
    love_graphics_pop();
    // Jokers: plain, foil, negative, polychrome
    static const float foil_xy[2] = {0.7f, 1.3f};
    Shader *wear[4] = {NULL, foil, negative, polychrome};
    for (int i = 0; i < 4; i++) {
        Quad_setViewport(cell, 71 * i, 0, 71, 95);
        if (wear[i]) {
            card_uniforms(wear[i], 0, false);
            Shader_send_vec(wear[i], i == 1 ? "foil" : i == 2 ? "negative" : "polychrome", foil_xy, 2);
        }
        love_graphics_setShader(wear[i]);
        love_graphics_draw_quad(jokers, cell, 150 + i * 90, 60, 0, 1, 1, 0, 0);
    }
    love_graphics_setShader(NULL);

    // flat things: a filled polygon, its outline, a rectangle under the text
    static const float star[10] = {70, 40, 110, 70, 95, 120, 45, 120, 30, 70};
    love_graphics_setColor(1, 0.8f, 0.1f, 1);
    love_graphics_polygon("fill", star, 5);
    love_graphics_setColor(0, 0, 0, 1);
    love_graphics_setLineWidth(3);
    love_graphics_polygon("line", star, 5);
    love_graphics_setColor(0, 0, 0, 0.6f);
    love_graphics_rectangle("fill", 40, 392, 560, 40);
    love_graphics_setColor(1, 1, 1, 1);
    love_graphics_setLineWidth(2);
    love_graphics_rectangle("line", 40, 392, 560, 40);
    float k = 520 / tw;
    love_graphics_setColor(0, 0, 0, 0.5f);
    love_graphics_draw_text(title, 62, 402, 0, k, k, 0, 0);
    love_graphics_setColor(1, 1, 1, 1);
    love_graphics_draw_text(title, 60, 400, 0, k, k, 0, 0);
    love_graphics_setColor(0.3f, 1, 0.4f, 1);
    love_graphics_print("love_graphics_print, size 12", 530, 20, 0.5f, 1, 1);
    love_graphics_setNewFont("resources/fonts/m6x11plus.ttf", 20);
    love_graphics_printf("printf, size 20", NULL, 20, 150);

    love_graphics_setCanvas(NULL);
    love_graphics_reset();
    love_graphics_clear(0, 0, 0, 1);
    love_graphics_draw(canvas, 0, 0, 0, 1, 1, 0, 0);
    printf("primitives: %d\n", love_graphics_count());
    love_present();
    CHECK(love_graphics_shot(out));
    Text_release(title);
}

int main(int argc, char **argv) {
    LoveConf conf = {.game = argc > 1 ? argv[1] : "/root/balatro-src/build/game", .pack = getenv("PACK"), .save_dir = getenv("SAVE_DIR"), .soft = true, .dt = 1 / 60.0f};
    love_init(&conf);
    CHECK(!strcmp(love_system_getOS(), "ps4") && love_timer_step() == conf.dt && love_timer_getFPS() == 60 && love_timer_getTime() >= 0);
    test_random();
    test_files();
    test_pad();
    test_sound();
    test_picture(argc > 2 ? argv[2] : "love_test.ppm");
    printf("PASSED\n");
    return 0;
}
