// game.lua:1-215: the Game class, Game:init and Game:start_up.
#include "all.h"

// game.lua:2 Game = Object:extend()
Class Game_class = {.name = "Game", .super = &Object_class};

// game.lua:5
void Game_init(Game *self) {
    G = self;

    Game_set_globals(self);
}

// Sprite(X, Y, W, H, atlas, pos)
static Sprite *new_sprite(float X, float Y, float W, float H, Atlas *atlas, Table *sprite_pos) {
    Sprite *s = NEW(Sprite);
    Sprite_init(s, X, Y, W, H, atlas, sprite_pos);
    return s;
}

// {x = x, y = y}
static Table *xy(num x, num y) { return TAB("x", N(x), "y", N(y)); }

// game.lua:50-52 and :152-154
static void clamp_texture_scaling(Game *self) {
    Table *GRAPHICS = ttab(self->SETTINGS, "GRAPHICS");
    Val texture_scaling = tget(GRAPHICS, "texture_scaling");
    if (truthy(texture_scaling)) {
        tsetn(GRAPHICS, "texture_scaling", vnum(texture_scaling) > 1 ? 2 : 1);
    }
}

// game.lua:11
void Game_start_up(Game *self) {
    // Load the settings file
    const char *settings = get_compressed("settings.jkr");
    Val settings_ver = NIL;
    if (settings) {
        Table *settings_file = STR_UNPACK(settings);
        Val version = tget(settings_file, "version");
        // note: Lua compares the strings with >= and <: strcmp
        if (strcmp(G->VERSION, "1.0.0") >= 0 && streq(love_system_getOS(), "NOPE") &&
            (!truthy(version) || strcmp(vstr(version), "1.0.0") < 0)) {
            for (int i = 1; i <= 3; i++) {
                love_filesystem_remove(fmt("%d/profile.jkr", i));
                love_filesystem_remove(fmt("%d/save.jkr", i));
                love_filesystem_remove(fmt("%d/meta.jkr", i));
                love_filesystem_remove(fmt("%d/unlock_notify.jkr", i));
                love_filesystem_remove(fmt("%d", i));
            }
            for (TIter it = titer(settings_file); tnext(&it);) {
                tsetv(self->SETTINGS, it.key, it.val);
            }
            tsetn(self->SETTINGS, "profile", 1);
            tset(self->SETTINGS, "tutorial_progress", NIL);
        } else {
            if (strcmp(G->VERSION, "1.0.0") < 0) {
                settings_ver = version;
            }
            for (TIter it = titer(settings_file); tnext(&it);) {
                tsetv(self->SETTINGS, it.key, it.val);
            }
        }
    }
    tset(self->SETTINGS, "version", truthy(settings_ver) ? settings_ver : S(G->VERSION));
    tset(self->SETTINGS, "paused", NIL);

    // game.lua:42 new_colour_proto = self.C["SO_"..(colourblind_option and 2 or 1)]
    if (tbool(self->SETTINGS, "colourblind_option")) {
        self->C.SUITS.Hearts = self->C.SO_2.Hearts;
        self->C.SUITS.Diamonds = self->C.SO_2.Diamonds;
        self->C.SUITS.Spades = self->C.SO_2.Spades;
        self->C.SUITS.Clubs = self->C.SO_2.Clubs;
    } else {
        self->C.SUITS.Hearts = self->C.SO_1.Hearts;
        self->C.SUITS.Diamonds = self->C.SO_1.Diamonds;
        self->C.SUITS.Spades = self->C.SO_1.Spades;
        self->C.SUITS.Clubs = self->C.SO_1.Clubs;
    }

    boot_timer("start", "settings", 0.1f);

    clamp_texture_scaling(self);

    if (tbool(self->SETTINGS, "DEMO") && !self->F_CTA) {
        tsett(self->SETTINGS, "DEMO",
              TAB("total_uptime", N(0), "timed_CTA_shown", B(true), "win_CTA_shown", B(true), "quit_CTA_shown", B(true)));
    }

    // create all sounds from resources and play one each to load into mem
    // note: the port has one SOURCES (io.h); sound_manager_init below, the thread's start, fills it again.
    SOURCES = tnew();
    int n_sound_files = 0;
    const char **sound_files = love_filesystem_getDirectoryItems("resources/sounds", &n_sound_files);

    for (int i = 0; i < n_sound_files; i++) {
        const char *filename = sound_files[i];
        const char *extension = str_sub(filename, -4, -1);
        if (streq(extension, ".ogg")) {
            const char *sound_code = str_sub(filename, 1, -5);
            tsett(SOURCES, sound_code, tnew());
        }
    }
    free(sound_files);

    if (!tbool(self->SETTINGS, "language")) tsets(self->SETTINGS, "language", "en-us");
    boot_timer("settings", "window init", 0.2f);
    Game_init_window(self, false);

    if (G->F_SOUND_THREAD) {
        boot_timer("window init", "soundmanager2", 0);
        // call the sound manager to prepare the thread to play sounds
        // note: game.h: the manager is an empty table (no thread, no channels); thread:start(1) is
        // sound_manager_init (io.h).
        self->SOUND_MANAGER = tnew();
        sound_manager_init();

        bool sound_loaded = false;
        const char *prev_file = "none";
        while (!sound_loaded && false) { // game.lua:90: never runs in the Lua either
            // Monitor the channel for any new requests
            Val request = Channel_pop(love_thread_getChannel("load_channel"));
            if (truthy(request)) {
                if (veq(request, S("finished"))) sound_loaded = true;
                else {
                    boot_timer(vstr(request), prev_file, 0);
                    prev_file = vstr(request);
                }
            }
            love_timer_sleep(0.001f);
        }

        boot_timer("soundmanager2", "savemanager", 0.22f);
    }

    boot_timer("window init", "savemanager", 0);
    // call the save manager to wait for any save requests
    // note: game.h: an empty table; the requests run inline (save_manager_request), nothing to start.
    G->SAVE_MANAGER = tnew();
    boot_timer("savemanager", "shaders", 0.4f);

    // call the http manager
    // note: game.h: an empty table; the HTTP manager is not ported (G.F_HTTP_SCORES thread:start(), game.lua:122-124).
    G->HTTP_MANAGER = tnew();

    // Load all shaders from resources
    self->SHADERS = tnew();
    int n_shader_files = 0;
    const char **shader_files = love_filesystem_getDirectoryItems("resources/shaders", &n_shader_files);
    for (int k = 0; k < n_shader_files; k++) {
        const char *filename = shader_files[k];
        const char *extension = str_sub(filename, -3, -1);
        if (streq(extension, ".fs")) {
            const char *shader_name = str_sub(filename, 1, -4);
            tset(self->SHADERS, shader_name, P(love_graphics_newShader(fmt("resources/shaders/%s", filename))));
        }
    }
    free(shader_files);

    boot_timer("shaders", "controllers", 0.7f);

    // Input handler/controller for game objects
    self->CONTROLLER = NEW(Controller);
    Controller_init(self->CONTROLLER);
    love_joystick_loadGamepadMappings("resources/gamecontrollerdb.txt");
    if (self->F_RUMBLE != 0) { // 0 as nil
        int n_joysticks = 0;
        Joystick **joysticks = love_joystick_getJoysticks(&n_joysticks);
        if (joysticks) {
            if (n_joysticks >= 1 && joysticks[0]) {
                Controller_set_gamepad(self->CONTROLLER, (n_joysticks >= 2 && joysticks[1]) ? joysticks[1] : joysticks[0]);
            }
        }
    }
    boot_timer("controllers", "localization", 0.8f);

    clamp_texture_scaling(self);

    Val profile = tget(G->SETTINGS, "profile");
    Game_load_profile(self, truthy(profile) ? (int)vnum(profile) : 1);

    tsett(self->SETTINGS, "QUEUED_CHANGE", tnew());
    tsett(self->SETTINGS, "music_control", TAB("desired_track", S(""), "current_track", S(""), "lerp", N(1)));

    Game_set_render_settings(self);

    Game_set_language(self);

    Game_init_item_prototypes(self);
    boot_timer("protos", "shared sprites", 0.9f);

    // For globally shared sprites
    float W = self->CARD_W, H = self->CARD_H;
    self->shared_debuff = new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), xy(4, 0));

    self->shared_soul = new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), ttab(ttab(self->P_CENTERS, "soul"), "pos"));
    self->shared_undiscovered_joker =
        new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), ttab(ttab(self->P_CENTERS, "undiscovered_joker"), "pos"));
    self->shared_undiscovered_tarot =
        new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), ttab(ttab(self->P_CENTERS, "undiscovered_tarot"), "pos"));

    self->shared_sticker_eternal = new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(0, 0));
    self->shared_sticker_perishable = new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(0, 2));
    self->shared_sticker_rental = new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(1, 2));

    // note: one statement per sprite, so they are created in the Lua constructor's order (C does not order arguments)
    self->shared_stickers = tnew();
    tseto(self->shared_stickers, "White", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(1, 0)));
    tseto(self->shared_stickers, "Red", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(2, 0)));
    tseto(self->shared_stickers, "Green", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(3, 0)));
    tseto(self->shared_stickers, "Black", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(0, 1)));
    tseto(self->shared_stickers, "Blue", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(4, 0)));
    tseto(self->shared_stickers, "Purple", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(1, 1)));
    tseto(self->shared_stickers, "Orange", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(2, 1)));
    tseto(self->shared_stickers, "Gold", new_sprite(0, 0, W, H, ASSET_ATLAS_get("stickers"), xy(3, 1)));
    self->shared_seals = tnew();
    tseto(self->shared_seals, "Gold", new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), xy(2, 0)));
    tseto(self->shared_seals, "Purple", new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), xy(4, 4)));
    tseto(self->shared_seals, "Red", new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), xy(5, 4)));
    tseto(self->shared_seals, "Blue", new_sprite(0, 0, W, H, ASSET_ATLAS_get("centers"), xy(6, 4)));
    self->sticker_map = ARR(S("White"), S("Red"), S("Green"), S("Black"), S("Blue"), S("Purple"), S("Orange"), S("Gold"));
    boot_timer("shared sprites", "prep stage", 0.95f);

    // For the visible cursor
    G->STAGE_OBJECT_INTERRUPT = true;
    self->CURSOR = new_sprite(0, 0, 0.3f, 0.3f, ASSET_ATLAS_get("gamepad_ui"), xy(18, 0));
    self->CURSOR->moveable.node.states.collide.can = false;
    G->STAGE_OBJECT_INTERRUPT = false;

    // Create the event manager for the game
    self->E_MANAGER = NEW(EventManager);
    EventManager_init(self->E_MANAGER);
    self->SPEEDFACTOR = 1;

    set_profile_progress();
    boot_timer("prep stage", "splash prep", 1);
    Game_splash_screen(self);
    boot_timer("splash prep", "end", 1);
}
