// game.lua:845-1372: Game:load_profile, set_language, set_render_settings, init_window, delete_run, save_*,
// prep_stage, sandbox (and the create_UIBox_sandbox_controls it defines).
#include "all.h"

// game.lua:914 recursive_init, the local closure of Game:load_profile
static void recursive_init(Table *t1, Table *t2) {
    for (TIter it = titer(t1); tnext(&it);) {
        Val cur = tgetv(t2, it.key);
        if (!truthy(cur)) {
            tsetv(t2, it.key, it.val);
        } else if (cur.t == V_TAB && it.val.t == V_TAB) {
            recursive_init(it.val.tab, cur.tab);
        }
    }
}

// game.lua:845 Game:load_profile(_profile)
void Game_load_profile(Game *self, int _profile) {
    if (!truthy(tgeti(G->PROFILES, _profile))) _profile = 1;
    tsetn(G->SETTINGS, "profile", _profile);

    // Load the settings file
    const char *info = get_compressed(fmt("%d/profile.jkr", _profile));
    if (info != NULL) {
        Table *profile = ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile"));
        for (TIter it = titer(STR_UNPACK(info)); tnext(&it);) {
            tsetv(profile, it.key, it.val);
        }
    }

    // note: the labels are string literals of this function in the Lua: LITERALS.profile of data/game.dat (data.h),
    // in the order of the source, which is the order below
    Table *x = lits("profile");
    Table *temp_profile = TAB(
        "MEMORY", T(TAB(
            "deck", S("Red Deck"),
            "stake", N(1))),
        "stake", N(1),

        "high_scores", T(TAB(
            "hand", T(TAB("label", S(lit_at(x, 1)), "amt", N(0))),
            "furthest_round", T(TAB("label", S(lit_at(x, 2)), "amt", N(0))),
            "furthest_ante", T(TAB("label", S(lit_at(x, 3)), "amt", N(0))),
            "most_money", T(TAB("label", S(lit_at(x, 4)), "amt", N(0))),
            "boss_streak", T(TAB("label", S(lit_at(x, 5)), "amt", N(0))),
            "collection", T(TAB("label", S(lit_at(x, 6)), "amt", N(0), "tot", N(1))),
            "win_streak", T(TAB("label", S(lit_at(x, 7)), "amt", N(0))),
            "current_streak", T(TAB("label", S(lit_at(x, 8)), "amt", N(0))),
            "poker_hand", T(TAB("label", S(lit_at(x, 9)), "amt", N(0))))),

        "career_stats", T(TAB(
            "c_round_interest_cap_streak", N(0),
            "c_dollars_earned", N(0),
            "c_shop_dollars_spent", N(0),
            "c_tarots_bought", N(0),
            "c_planets_bought", N(0),
            "c_playing_cards_bought", N(0),
            "c_vouchers_bought", N(0),
            "c_tarot_reading_used", N(0),
            "c_planetarium_used", N(0),
            "c_shop_rerolls", N(0),
            "c_cards_played", N(0),
            "c_cards_discarded", N(0),
            "c_losses", N(0),
            "c_wins", N(0),
            "c_rounds", N(0),
            "c_hands_played", N(0),
            "c_face_cards_played", N(0),
            "c_jokers_sold", N(0),
            "c_cards_sold", N(0),
            "c_single_hand_round_streak", N(0))),
        "progress", T(tnew()),
        "joker_usage", T(tnew()),
        "consumeable_usage", T(tnew()),
        "voucher_usage", T(tnew()),
        "hand_usage", T(tnew()),
        "deck_usage", T(tnew()),
        "deck_stakes", T(tnew()),
        // challenges_unlocked = nil
        "challenge_progress", T(TAB(
            "completed", T(tnew()),
            "unlocked", T(tnew()))));

    recursive_init(temp_profile, ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")));
}

// self.LANGUAGES as the Lua's table constructor makes it (game.lua:943-957): [key] = {font = <number>, label = , key = ,
// [beta = true,] button = , warning = {...}} and the two `omit` entries. The texts are the game's: the table is the
// `language` section of data/game.dat (data.h), which was taken after the Lua's last loop put the FONTS entry in
// `font`; here it is the number again, for the same loop below.
static Table *languages_from_data(void) {
    Table *dat = ttab(game_dat(), "language"), *fonts = ttab(dat, "FONTS"), *languages = tnew();
    if (!ttab(dat, "LANGUAGES") || !fonts) ERROR("data/game.dat has no language.LANGUAGES: regenerate it with tools/dump_data.sh");
    for (TIter it = titer(ttab(dat, "LANGUAGES")); tnext(&it);) {
        Table *v = tnew();
        for (TIter f = titer(it.val.tab); tnext(&f);) {
            Val val = f.val;
            if (veq(f.key, S("font"))) {
                int n = 1;
                while (ttabi(fonts, n) && ttabi(fonts, n) != val.tab) n++;
                if (!ttabi(fonts, n)) ERROR("data/game.dat: the font of language '%s' is not one of FONTS", vstr(it.key));
                val = N(n);
            } else if (val.t == V_TAB) {
                val = T(tcopy(val.tab));
            }
            tsetv(v, f.key, val);
        }
        tsetv(languages, it.key, T(v));
    }
    return languages;
}

// One entry of self.FONTS (game.lua:969-977). FONTSCALE-independent fields (squish = 1) are the same in all nine.
static Val font_entry(const char *file, float render_scale, num TEXT_HEIGHT_SCALE, num ox, num oy, num FONTSCALE, num DESCSCALE) {
    return T(TAB("file", S(file), "render_scale", N(render_scale), "TEXT_HEIGHT_SCALE", N(TEXT_HEIGHT_SCALE),
                 "TEXT_OFFSET", T(TAB("x", N(ox), "y", N(oy))), "FONTSCALE", N(FONTSCALE), "squish", N(1),
                 "DESCSCALE", N(DESCSCALE)));
}

// game.lua:927 Game:set_language()
void Game_set_language(Game *self) {
    if (!self->LANGUAGES) {
        void *file = love_filesystem_read(fmt("localization/%s.lua", tstr(G->SETTINGS, "language")), NULL);
        bool readable = file != NULL;
        free(file);
        if (!readable || G->F_ENGLISH_ONLY) {
            // SET LANGUAGE FOR FIRST TIME STARTUP HERE
            tsets(G->SETTINGS, "language", "en-us");
        }
        // IF LANGUAGE NEEDS TO BE SET ON EVERY REBOOT, SET HERE

        self->LANGUAGES = languages_from_data();

        // load the font and set filter
        self->FONTS = ARR(
            font_entry("resources/fonts/m6x11plus.ttf", self->TILESIZE * 10, 0.83, 10, -20, 0.1, 1),
            font_entry("resources/fonts/NotoSansSC-Bold.ttf", self->TILESIZE * 7, 0.7, 0, -35, 0.12, 1.1),
            font_entry("resources/fonts/NotoSansTC-Bold.ttf", self->TILESIZE * 7, 0.7, 0, -35, 0.12, 1.1),
            font_entry("resources/fonts/NotoSansKR-Bold.ttf", self->TILESIZE * 7, 0.8, 0, -20, 0.12, 1),
            font_entry("resources/fonts/NotoSansJP-Bold.ttf", self->TILESIZE * 7, 0.8, 0, -20, 0.12, 1),
            font_entry("resources/fonts/NotoSans-Bold.ttf", self->TILESIZE * 7, 0.65, 0, -40, 0.12, 1),
            font_entry("resources/fonts/m6x11plus.ttf", self->TILESIZE * 10, 0.9, 10, 15, 0.1, 1),
            font_entry("resources/fonts/GoNotoCurrent-Bold.ttf", self->TILESIZE * 10, 0.8, 10, -20, 0.1, 1),
            font_entry("resources/fonts/GoNotoCJKCore.ttf", self->TILESIZE * 10, 0.8, 10, -20, 0.1, 1));
        for (int i = 1; truthy(tgeti(self->FONTS, i)); i++) {
            Table *v = ttabi(self->FONTS, i);
            if (love_filesystem_getInfo(tstr(v, "file"), NULL)) {
                tset(v, "FONT", P(love_graphics_newFont(tstr(v, "file"), (float)tnum(v, "render_scale"))));
            }
        }
        for (TIter it = titer(self->LANGUAGES); tnext(&it);) {
            Table *v = it.val.tab;
            tset(v, "font", tgeti(self->FONTS, (int)tnum(v, "font")));
        }
    }

    self->LANG = ttab(self->LANGUAGES, tstr(self->SETTINGS, "language"));
    if (!self->LANG) self->LANG = ttab(self->LANGUAGES, "en-us");

    bool localization = love_filesystem_getInfo(fmt("localization/%s.lua", tstr(G->SETTINGS, "language")), NULL);
    if (localization) {
        // note: there is no loadstring: the table the Lua file returns is data/loc_<language>.dat (data.h); the
        // assert is the ERROR.
        self->localization = data_load(fmt("data/loc_%s.dat", tstr(G->SETTINGS, "language")));
        tfreeze(&self->localization); // (the collector does not walk it at every collection; again after a change of language)
        if (!self->localization) ERROR("data/loc_%s.dat cannot be read", tstr(G->SETTINGS, "language"));
        init_localization();
    }
}

// {name = , path = "resources/textures/<texture_scaling>x/<file>", px = , py = } of game.lua:1010-1080
static Table *atlas_def(const char *name, const char *file, num px, num py) {
    return TAB("name", S(name),
               "path", S(fmt("resources/textures/%sx/%s", tostring(tget(ttab(G->SETTINGS, "GRAPHICS"), "texture_scaling")), file)),
               "px", N(px), "py", N(py));
}

// game.lua:998 Game:set_render_settings()
void Game_set_render_settings(Game *self) {
    Table *GRAPHICS = ttab(self->SETTINGS, "GRAPHICS");
    if (!truthy(tget(GRAPHICS, "texture_scaling"))) tsetn(GRAPHICS, "texture_scaling", 2);

    // Set fiter to linear interpolation and nearest, best for pixel art
    // (the anisotropy argument, 1, has no place in the platform's function)
    love_graphics_setDefaultFilter(
        veq(tget(GRAPHICS, "texture_scaling"), N(1)) ? "nearest" : "linear",
        veq(tget(GRAPHICS, "texture_scaling"), N(1)) ? "nearest" : "linear");

    love_graphics_setLineStyle("rough");

    // spritesheets
    Table *blind_chips = atlas_def("blind_chips", "BlindChips.png", 34, 34);
    tsetn(blind_chips, "frames", 21);
    Table *shop_sign = atlas_def("shop_sign", "ShopSignAnimation.png", 113, 57);
    tsetn(shop_sign, "frames", 4);
    self->animation_atli = ARR(T(blind_chips), T(shop_sign));

    self->asset_atli = ARR(
        T(atlas_def("cards_1", "8BitDeck.png", 71, 95)),
        T(atlas_def("cards_2", "8BitDeck_opt2.png", 71, 95)),
        T(atlas_def("centers", "Enhancers.png", 71, 95)),
        T(atlas_def("Joker", "Jokers.png", 71, 95)),
        T(atlas_def("Tarot", "Tarots.png", 71, 95)),
        T(atlas_def("Voucher", "Vouchers.png", 71, 95)),
        T(atlas_def("Booster", "boosters.png", 71, 95)),
        T(atlas_def("ui_1", "ui_assets.png", 18, 18)),
        T(atlas_def("ui_2", "ui_assets_opt2.png", 18, 18)),
        T(atlas_def("balatro", "balatro.png", 333, 216)),
        T(atlas_def("gamepad_ui", "gamepad_ui.png", 32, 32)),
        T(atlas_def("icons", "icons.png", 66, 66)),
        T(atlas_def("tags", "tags.png", 34, 34)),
        T(atlas_def("stickers", "stickers.png", 71, 95)),
        T(atlas_def("chips", "chips.png", 29, 29)));
    // note: game.lua:1030-1080 writes the 48 collab entries one by one; they are collab_<X>_1 and collab_<X>_2 of
    // collabs/, 71 x 95, for these X in this order.
    static const char *const collabs[] = {"AU", "TW", "VS", "DTD", "CYP", "STS", "TBoI", "SV", "SK", "DS", "CL", "D2",
                                          "PC", "WF", "EG", "XR", "CR", "BUG", "FO", "DBD", "C7", "R", "AC", "STP"};
    for (size_t c = 0; c < sizeof(collabs) / sizeof(collabs[0]); c++) {
        for (int n = 1; n <= 2; n++) {
            const char *name = fmt("collab_%s_%d", collabs[c], n);
            tinsert(self->asset_atli, T(atlas_def(name, fmt("collabs/%s.png", name), 71, 95)));
        }
    }
    self->asset_images = ARR(
        T(TAB("name", S("playstack_logo"), "path", S("resources/textures/1x/playstack-logo.png"), "px", N(1417), "py", N(1417))),
        T(TAB("name", S("localthunk_logo"), "path", S("resources/textures/1x/localthunk-logo.png"), "px", N(1390), "py", N(560))));

    // Load in all atli defined above
    // note: an atlas is a new Atlas each call, as the Lua's new table; the old ones are not freed (Sprite_reset
    // rebinds every live sprite by name below).
    for (int i = 1; i <= tlen(self->animation_atli); i++) {
        Table *def = ttabi(self->animation_atli, i);
        Atlas *atlas = NEWCTX(Atlas);
        tset(self->ANIMATION_ATLAS, tstr(def, "name"), P(atlas));
        atlas->name = tstr(def, "name");
        atlas->image = love_graphics_newImage(tstr(def, "path"), (float)tnum(GRAPHICS, "texture_scaling")); // {mipmaps = true, dpiscale = texture_scaling}
        atlas->px = (float)tnum(def, "px");
        atlas->py = (float)tnum(def, "py");
        atlas->frames = (int)tnum(def, "frames");
    }

    for (int i = 1; i <= tlen(self->asset_atli); i++) {
        Table *def = ttabi(self->asset_atli, i);
        Atlas *atlas = NEWCTX(Atlas);
        tset(self->ASSET_ATLAS, tstr(def, "name"), P(atlas));
        atlas->name = tstr(def, "name");
        atlas->image = love_graphics_newImage(tstr(def, "path"), (float)tnum(GRAPHICS, "texture_scaling"));
        atlas->type = tstr(def, "type");
        atlas->px = (float)tnum(def, "px");
        atlas->py = (float)tnum(def, "py");
    }
    for (int i = 1; i <= tlen(self->asset_images); i++) {
        Table *def = ttabi(self->asset_images, i);
        Atlas *atlas = NEWCTX(Atlas);
        tset(self->ASSET_ATLAS, tstr(def, "name"), P(atlas));
        atlas->name = tstr(def, "name");
        atlas->image = love_graphics_newImage(tstr(def, "path"), 1); // {mipmaps = true, dpiscale = 1}
        atlas->type = tstr(def, "type");
        atlas->px = (float)tnum(def, "px");
        atlas->py = (float)tnum(def, "py");
    }

    for (TIter it = titer(G->I.SPRITE); tnext(&it);) {
        CALL(it.val.obj, reset);
    }

    tset(self->ASSET_ATLAS, "Planet", tget(self->ASSET_ATLAS, "Tarot"));
    tset(self->ASSET_ATLAS, "Spectral", tget(self->ASSET_ATLAS, "Tarot"));
}

// game.lua:1122 Game:init_window(reset)
void Game_init_window(Game *self, bool reset) {
    // Initialize the window
    self->ROOM_PADDING_H = 0.7f;
    self->ROOM_PADDING_W = 1;
    self->WINDOWTRANS.x = 0;
    self->WINDOWTRANS.y = 0;
    self->WINDOWTRANS.w = self->TILE_W + 2 * self->ROOM_PADDING_W;
    self->WINDOWTRANS.h = self->TILE_H + 2 * self->ROOM_PADDING_H;
    self->window_prev.orig_scale = self->TILESCALE;
    self->window_prev.w = self->WINDOWTRANS.w * self->TILESIZE * self->TILESCALE;
    self->window_prev.h = self->WINDOWTRANS.h * self->TILESIZE * self->TILESCALE;
    self->window_prev.orig_ratio =
        self->WINDOWTRANS.w * self->TILESIZE * self->TILESCALE / (self->WINDOWTRANS.h * self->TILESIZE * self->TILESCALE);
    if (!ttab(G->SETTINGS, "QUEUED_CHANGE")) tsett(G->SETTINGS, "QUEUED_CHANGE", tnew());
    tset(ttab(G->SETTINGS, "QUEUED_CHANGE"), "screenmode", tget(ttab(G->SETTINGS, "WINDOW"), "screenmode"));

    FUNCS_apply_window_changes(true);
}

// `if self.x then self.x:remove(); self.x = nil end` of game.lua:1148-1167
#define REMOVE_FIELD(field) \
    do { \
        if (self->field) { \
            CALL(self->field, remove); \
            self->field = NULL; \
        } \
    } while (0)

// game.lua:1142 Game:delete_run()
void Game_delete_run(Game *self) {
    if (self->ROOM) {
        remove_all(ttabi(G->STAGE_OBJECTS, G->STAGE));
        self->load_shop_booster = NULL;
        self->load_shop_jokers = NULL;
        self->load_shop_vouchers = NULL;
        REMOVE_FIELD(buttons);
        REMOVE_FIELD(deck_preview);
        REMOVE_FIELD(shop);
        REMOVE_FIELD(blind_select);
        REMOVE_FIELD(booster_pack);
        REMOVE_FIELD(MAIN_MENU_UI);
        REMOVE_FIELD(SPLASH_FRONT);
        REMOVE_FIELD(SPLASH_BACK);
        REMOVE_FIELD(SPLASH_LOGO);
        REMOVE_FIELD(GAME_OVER_UI);
        REMOVE_FIELD(collection_alert);
        REMOVE_FIELD(HUD);
        REMOVE_FIELD(HUD_blind);
        if (self->HUD_tags) {
            for (TIter it = titer(self->HUD_tags); tnext(&it);) {
                CALL(it.val.obj, remove);
            }
            self->HUD_tags = NULL;
        }
        REMOVE_FIELD(OVERLAY_MENU);
        if (self->OVERLAY_TUTORIAL) {
            CALL(tobj(G->OVERLAY_TUTORIAL->moveable.node.object.dyn, "Jimbo"), remove);
            void *content = tobj(G->OVERLAY_TUTORIAL->moveable.node.object.dyn, "content");
            if (content) CALL(content, remove);
            CALL(G->OVERLAY_TUTORIAL, remove);
            G->OVERLAY_TUTORIAL = NULL;
        }
        // note: the Lua scans pairs(G) for every value that is a CardArea: these are the members of G that can hold
        // one (game.h: G_CARDAREA_NAMES, title_top, and your_collection when it is a single area).
        for (int i = 0; i < G_CARDAREA_COUNT; i++) {
            *Game_cardarea(G, G_CARDAREA_NAMES[i]) = NULL;
        }
        G->title_top = NULL;
        if (G->your_collection.t == V_OBJ && is_a(G->your_collection.obj, &CardArea_class)) G->your_collection = NIL;
        G->I.CARD = tnew();
    }
    G->VIEWING_DECK = false;
    EventManager_clear_queue(G->E_MANAGER, NULL, NULL);
    Controller_mod_cursor_context_layer(G->CONTROLLER, -1000);
    tsett(G->CONTROLLER->object.dyn, "focus_cursor_stack", tnew());
    tsetn(G->CONTROLLER->object.dyn, "focus_cursor_stack_level", 1);
    if (G->GAME) tsetb(G->GAME, "won", false);

    G->STATE = -1;
}

// The loop body of game.lua:1199-1210, the same for the four prototype tables
static void save_progress_UDA(Table *UDA, Table *prototypes) {
    for (TIter it = titer(prototypes); tnext(&it);) {
        Table *v = it.val.tab;
        tsetv(UDA, it.key, S(fmt("%s%s%s", tbool(v, "unlocked") ? "u" : "", tbool(v, "discovered") ? "d" : "", tbool(v, "alerted") ? "a" : "")));
    }
}

// game.lua:1193 Game:save_progress()
void Game_save_progress(Game *self) {
    if (!ttab(G->ARGS, "save_progress")) tsett(G->ARGS, "save_progress", tnew());
    Table *save_progress = ttab(G->ARGS, "save_progress");
    tsett(save_progress, "UDA", EMPTY(ttab(save_progress, "UDA")));
    tsett(save_progress, "SETTINGS", G->SETTINGS);
    tset(save_progress, "PROFILE", tgeti(G->PROFILES, (int)tnum(G->SETTINGS, "profile")));

    Table *UDA = ttab(save_progress, "UDA");
    save_progress_UDA(UDA, self->P_CENTERS);
    save_progress_UDA(UDA, self->P_BLINDS);
    save_progress_UDA(UDA, self->P_TAGS);
    save_progress_UDA(UDA, self->P_SEALS);

    if (!G->FILE_HANDLER) G->FILE_HANDLER = tnew();
    tsetb(G->FILE_HANDLER, "progress", true);
    tsetb(G->FILE_HANDLER, "update_queued", true);
}

// game.lua:1217 Game:save_notify(card)
void Game_save_notify(Game *self, Table *card) {
    save_manager_request(TAB(
        "type", S("save_notify"),
        "save_notify", tget(card, "key"),
        "profile_num", tget(G->SETTINGS, "profile")));
}

// game.lua:1225 Game:save_settings()
void Game_save_settings(Game *self) {
    tsett(G->ARGS, "save_settings", G->SETTINGS);
    if (!G->FILE_HANDLER) G->FILE_HANDLER = tnew();
    tsetb(G->FILE_HANDLER, "settings", true);
    tsetb(G->FILE_HANDLER, "update_queued", true);
}

// game.lua:1232 Game:save_metrics()
void Game_save_metrics(Game *self) {
    tsett(G->ARGS, "save_metrics", G->METRICS);
    if (!G->FILE_HANDLER) G->FILE_HANDLER = tnew();
    tsetb(G->FILE_HANDLER, "settings", true); // (sic: the Lua sets settings, not metrics)
    tsetb(G->FILE_HANDLER, "update_queued", true);
}

// game.lua:1239 Game:prep_stage(new_stage, new_state, new_game_obj)
void Game_prep_stage(Game *self, int new_stage, int new_state, bool new_game_obj) {
    tclear(self->CONTROLLER->locks); // for k, v in pairs(locks) do locks[k] = nil end
    if (new_game_obj) self->GAME = Game_init_game_object(self);
    self->STAGE = new_stage ? new_stage : STAGES_MAIN_MENU;
    self->STATE = new_state ? new_state : STATES_MENU;
    self->STATE_COMPLETE = false;
    tsetb(self->SETTINGS, "paused", false);

    Node *ROOM = NEW(Node);
    Node_init(ROOM, TAB("T", T(TAB(
        "x", N(self->ROOM_PADDING_W),
        "y", N(self->ROOM_PADDING_H),
        "w", N(self->TILE_W),
        "h", N(self->TILE_H)))));
    self->ROOM = ROOM;
    tsetn(self->ROOM->object.dyn, "jiggle", 0);
    self->ROOM->states.drag.can = false;
    CALL(self->ROOM, set_container, self->ROOM);

    Moveable *ROOM_ATTACH = NEW(Moveable);
    Moveable_init_args(ROOM_ATTACH, TAB("T", T(TAB(
        "x", N(0),
        "y", N(0),
        "w", N(self->TILE_W),
        "h", N(self->TILE_H)))));
    self->ROOM_ATTACH = ROOM_ATTACH;
    self->ROOM_ATTACH->node.states.drag.can = false;
    CALL(self->ROOM_ATTACH, set_container, self->ROOM);
    love_resize((float)love_graphics_getWidth(), (float)love_graphics_getHeight());
}

// G.C[name] for the names of G.SANDBOX.col_op (game.lua:1316, :1319). note: game.h has no G.C lookup by name; any
// other name is nil.
static float *sandbox_colour(const char *name) {
    if (streq(name, "RED")) return G->C.RED;
    if (streq(name, "BLUE")) return G->C.BLUE;
    if (streq(name, "GREEN")) return G->C.GREEN;
    if (streq(name, "BLACK")) return G->C.BLACK;
    if (streq(name, "L_BLACK")) return G->C.L_BLACK;
    if (streq(name, "WHITE")) return G->C.WHITE;
    if (streq(name, "EDITION")) return G->C.EDITION;
    if (streq(name, "DARK_EDITION")) return G->C.DARK_EDITION;
    if (streq(name, "ORANGE")) return G->C.ORANGE;
    if (streq(name, "PURPLE")) return G->C.PURPLE;
    return NULL;
}

// game.lua:1315 G.FUNCS.col1change
static void FUNCS_col1change(Table *args) {
    float *colour = sandbox_colour(tstr(args, "to_val"));
    tset(G->SANDBOX, "col1", colour ? P(colour) : NIL);
}

// game.lua:1318 G.FUNCS.col2change
static void FUNCS_col2change(Table *args) {
    float *colour = sandbox_colour(tstr(args, "to_val"));
    tset(G->SANDBOX, "col2", colour ? P(colour) : NIL);
}

// game.lua:1321 G.FUNCS.edition_change
static void FUNCS_edition_change(Table *args) {
    tset(G->SANDBOX, "edition", tget(args, "to_val"));
    if (tobj(G->SANDBOX, "joker")) {
        Table *edition = tnew();
        tsetv(edition, tget(args, "to_val"), B(true));
        Card_set_edition(tobj(G->SANDBOX, "joker"), edition, true, true);
    }
}

// game.lua:1325 G.FUNCS.pulseme
static void FUNCS_pulseme(UIElement *e) {
    if (math_random() > 0.998) DynaText_pulse(tobj(e->moveable.node.config, "object"), 1);
}

// game.lua:1329 G.FUNCS.rem_joker
static void FUNCS_rem_joker(UIElement *e) {
    if (tobj(G->SANDBOX, "joker")) {
        CALL(tobj(G->SANDBOX, "joker"), remove);
        tset(G->SANDBOX, "joker", NIL);
    }
}

// game.lua:1328 G.FUNCS.spawn_joker
static void FUNCS_spawn_joker(UIElement *e) {
    FUNCS_rem_joker(NULL);
    tseto(G->SANDBOX, "joker", add_joker(tstr(G->SANDBOX, "joker_text"), tstr(G->SANDBOX, "edition"), false, false));
}

// game.lua:1330 G.FUNCS.do_time
static void FUNCS_do_time(Table *args) {
    if (streq(tstr(args, "to_val"), "PLAY")) tsetn(G->SANDBOX, "gamespeed", 1);
    else tsetn(G->SANDBOX, "gamespeed", 0);
}

// game.lua:1331 G.FUNCS.cb
static void FUNCS_cb(Table *rt) {
    G->CARD_W = (float)(vnum(ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value"))) * tnum(ttab(G->SANDBOX, "base_size"), "w"));
    G->CARD_H = (float)(vnum(ui_ref_get(tget(rt, "ref_table"), tget(rt, "ref_value"))) * tnum(ttab(G->SANDBOX, "base_size"), "h"));
}

// game.lua:1333 the event of create_UIBox_sandbox_controls (it returns nil: it never completes)
static bool ev_sandbox_tick(void *ctx) {
    Val file_reload_timer = tget(G->SANDBOX, "file_reload_timer");
    tsetn(G->SANDBOX, "file_reload_timer", truthy(file_reload_timer) ? file_reload_timer.n : 0);
    if (tnum(G->SANDBOX, "file_reload_timer") < G->TIMERS.REAL) {
        tsetn(G->SANDBOX, "file_reload_timer", tnum(G->SANDBOX, "file_reload_timer") + 0.25);
    }
    if (tobj(G->SANDBOX, "joker")) ((Card *)tobj(G->SANDBOX, "joker"))->ambient_tilt = (float)tnum(G->SANDBOX, "tilt");
    tsetn(G->SANDBOX, "vort_time", tnum(G->SANDBOX, "vort_time") + G->real_dt * tnum(G->SANDBOX, "gamespeed"));
    tsetb(G->CONTROLLER->object.dyn, "lock_input", false);
    return false;
}

// game.lua:1314 create_UIBox_sandbox_controls()
Table *create_UIBox_sandbox_controls(void) {
    funcs_register_args("col1change", FUNCS_col1change);
    funcs_register_args("col2change", FUNCS_col2change);
    funcs_register_args("edition_change", FUNCS_edition_change);
    funcs_register("pulseme", FUNCS_pulseme);
    funcs_register("spawn_joker", FUNCS_spawn_joker);
    funcs_register("rem_joker", FUNCS_rem_joker);
    funcs_register_args("do_time", FUNCS_do_time);
    funcs_register_args("cb", FUNCS_cb);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_sandbox_tick}), NULL, false);

    // note: the labels are literals of the Lua: LITERALS.sandbox of data/game.dat (data.h), in the order of the source
    Table *t = TAB(
        "n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "r", N(0.1), "emboss", N(0.1), "colour", P(G->C.L_BLACK))), "nodes", T(ARR(
                T(create_slider(TAB("label", S(lit("sandbox", 1)), "w", N(2), "h", N(0.3), "text_scale", N(0.2), "label_scale", N(0.3), "ref_table", T(G->SANDBOX), "ref_value", S("vort_time"), "min", N(0), "max", N(30)))),
                T(create_option_cycle(TAB("options", T(ARR(S(lit("sandbox", 2)), S(lit("sandbox", 3)))), "opt_callback", S("do_time"), "current_option", N(1), "colour", P(G->C.RED), "w", N(2), "scale", N(0.7)))),
                T(create_slider(TAB("label", S(lit("sandbox", 4)), "w", N(2), "h", N(0.3), "text_scale", N(0.2), "label_scale", N(0.3), "ref_table", T(G->SANDBOX), "ref_value", S("tilt"), "min", N(0), "max", N(3), "decimal_places", N(2)))),
                T(create_slider(TAB("label", S(lit("sandbox", 5)), "w", N(2), "h", N(0.3), "text_scale", N(0.2), "label_scale", N(0.3), "ref_table", T(G->SANDBOX), "ref_value", S("card_size"), "min", N(0.1), "max", N(3), "callback", S("cb"), "decimal_places", N(2)))),
                T(create_option_cycle(TAB("options", T(ttab(G->SANDBOX, "col_op")), "opt_callback", S("col1change"), "current_option", N(1), "colour", P(G->C.RED), "w", N(2), "scale", N(0.7)))),
                T(create_option_cycle(TAB("options", T(ttab(G->SANDBOX, "col_op")), "opt_callback", S("col2change"), "current_option", N(2), "colour", P(G->C.RED), "w", N(2), "scale", N(0.7)))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
                    T(UIBox_button(TAB("label", T(ARR(S("+"))), "button", S("spawn_joker"), "minw", N(0.7), "col", B(true)))),
                    T(create_text_input(TAB("prompt_text", S(lit("sandbox", 6)), "extended_corpus", B(true), "ref_table", T(G->SANDBOX), "ref_value", S("joker_text"), "text_scale", N(0.3), "w", N(1.5), "h", N(0.6)))),
                    T(UIBox_button(TAB("label", T(ARR(S("-"))), "button", S("rem_joker"), "minw", N(0.7), "col", B(true)))))))),
                T(create_option_cycle(TAB("options", T(ARR(S("base"), S("foil"), S("holo"), S("polychrome"), S("negative"))), "opt_callback", S("edition_change"), "current_option", N(1), "colour", P(G->C.RED), "w", N(2), "scale", N(0.7)))))))))));
    return t;
}

// game.lua:1270 Game:sandbox()
void Game_sandbox(Game *self) {
    G->TIMERS.REAL = 0;
    G->TIMERS.TOTAL = 0;

    Game_prep_stage(self, STAGES_SANDBOX, STATES_SANDBOX, true);
    Back *selected_back = NEW(Back);
    Back_init(selected_back, ttab(G->P_CENTERS, "b_red"));
    tseto(self->GAME, "selected_back", selected_back);

    ease_background_colour(TAB("new_colour", P(G->C.BLACK), "contrast", N(1)));

    G->SANDBOX = TAB(
        "vort_time", N(7),
        "vort_speed", N(0),
        "col_op", T(ARR(S("RED"), S("BLUE"), S("GREEN"), S("BLACK"), S("L_BLACK"), S("WHITE"), S("EDITION"), S("DARK_EDITION"), S("ORANGE"), S("PURPLE"))),
        "col1", P(G->C.RED), "col2", P(G->C.BLUE),
        "mid_flash", N(0),
        "joker_text", S(""),
        "edition", S("base"),
        "tilt", N(1),
        "card_size", N(1),
        "base_size", T(TAB("w", N(G->CARD_W), "h", N(G->CARD_H))),
        "gamespeed", N(0));

    REMOVE_FIELD(SPLASH_FRONT);
    REMOVE_FIELD(SPLASH_BACK);

    Sprite *SPLASH_BACK = NEW(Sprite);
    Sprite_init(SPLASH_BACK, -30, -13, G->ROOM->T->w + 60, G->ROOM->T->h + 22,
                ASSET_ATLAS_get(fmt("ui_%d", tbool(G->SETTINGS, "colourblind_option") ? 2 : 1)), TAB("x", N(2), "y", N(0)));
    G->SPLASH_BACK = SPLASH_BACK;
    Moveable_set_alignment((Moveable *)G->SPLASH_BACK, TAB(
        "major", O(G->ROOM_ATTACH),
        "type", S("cm"),
        "offset", T(TAB("x", N(0), "y", N(0)))));

    Sprite_define_draw_steps(G->SPLASH_BACK, ARR(T(TAB(
        "shader", S("splash"),
        "send", T(ARR(
            T(TAB("name", S("time"), "ref_table", T(G->SANDBOX), "ref_value", S("vort_time"))),
            T(TAB("name", S("vort_speed"), "val", N(0.4))),
            T(TAB("name", S("colour_1"), "ref_table", T(G->SANDBOX), "ref_value", S("col1"))),
            T(TAB("name", S("colour_2"), "ref_table", T(G->SANDBOX), "ref_value", S("col2"))),
            T(TAB("name", S("mid_flash"), "ref_table", T(G->SANDBOX), "ref_value", S("mid_flash"))),
            T(TAB("name", S("vort_offset"), "val", N(0)))))))));

    // (create_UIBox_sandbox_controls is defined here in the Lua: the function above)

    UIBox *UI = UIBox_new(TAB(
        "definition", T(create_UIBox_sandbox_controls()),
        "config", T(TAB("align", S("cli"), "offset", T(TAB("x", N(0), "y", N(0))), "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
    tseto(G->SANDBOX, "UI", UI);

    UIBox_recalculate(UI);
}
