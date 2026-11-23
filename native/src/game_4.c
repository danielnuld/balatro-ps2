// game.lua:1373-2017: Game:splash_screen, make_splash_card, Game:main_menu, Game:demo_cta, Game:init_game_object.
#include "all.h"
#include <time.h> // note: os.time() (game.lua:1404) is libc's time(NULL); love.h has no os_time

// ---------------------------------------------------------------- local helpers
// Sprite(X, Y, W, H, atlas, {x = px, y = py})
static Sprite *sprite_new(float X, float Y, float W, float H, Atlas *atlas, num px, num py) {
    Sprite *s = NEW(Sprite);
    Sprite_init(s, X, Y, W, H, atlas, TAB("x", N(px), "y", N(py)));
    return s;
}

// Card(X, Y, W, H, card, center)
static Card *card_new(float X, float Y, float W, float H, Table *card, Table *center) {
    Card *c = NEW(Card);
    Card_init(c, X, Y, W, H, card, center, NULL);
    return c;
}

// note: the Lua shares a card's states.hover / click / drag tables with its front, center and back sprites
// (card.lua:155-157); States is by value in node.h, so a write to the card's is copied to them here.
static void mirror_states(Card *card) {
    static const char *const names[] = {"front", "center", "back"};
    for (int i = 0; i < 3; i++) {
        Sprite *s = Card_child(card, names[i]);
        if (!s) continue;
        s->moveable.node.states.hover = card->moveable.node.states.hover;
        s->moveable.node.states.click = card->moveable.node.states.click;
        s->moveable.node.states.drag = card->moveable.node.states.drag;
    }
}

// {major = major, type = 'cm', [bond = bond,] offset = {x=0,y=0}}
static Table *cm_alignment(void *major, const char *bond) {
    Table *t = TAB("major", O(major), "type", S("cm"));
    if (bond) tsets(t, "bond", bond);
    tsett(t, "offset", TAB("x", N(0), "y", N(0)));
    return t;
}

// {G.C.WHITE, G.C.WHITE}
static Table *white_white(void) { return ARR(P(G->C.WHITE), P(G->C.WHITE)); }

// ---------------------------------------------------------------- Game:splash_screen
// game.lua:1382
static bool ev_splash_discover(void *ctx) {
    discover_card(NULL);
    return true;
}

// `local SC = nil` (game.lua:1425): written by one event, read by the next: shared through this context
struct splash_sc_ctx { Card *SC; };

// game.lua:1426
static bool ev_splash_card_in(void *ctx) {
    struct splash_sc_ctx *c = ctx;
    num SC_scale = 1.2;
    Card *SC = card_new(G->ROOM->T->w / 2 - SC_scale * G->CARD_W / 2, 10. + G->ROOM->T->h / 2 - SC_scale * G->CARD_H / 2,
                        SC_scale * G->CARD_W, SC_scale * G->CARD_H, NULL /* G.P_CARDS.empty: nil */, ttab(G->P_CENTERS, "j_joker"));
    c->SC = SC;
    SC->moveable.node.T->y = G->ROOM->T->h / 2 - SC_scale * G->CARD_H / 2;
    SC->ambient_tilt = 1;
    SC->moveable.node.states.drag.can = false;
    SC->moveable.node.states.hover.can = false;
    mirror_states(SC);
    SC->no_ui = true;
    G->VIBRATION = G->VIBRATION + 2;
    play_sound("whoosh1", 0.7f, 0.2f);
    play_sound("introPad1", 0.704f, 0.6f);
    return true;
}

// game.lua:1440
static bool ev_splash_card_dissolve(void *ctx) {
    struct splash_sc_ctx *c = ctx;
    Card_start_dissolve(c->SC, white_white(), true, 12, true);
    play_sound("magic_crumple", 1, 0.5f);
    play_sound("splash_buildup", 1, 0.7f);
    return true;
}

// game.lua:1447 make_splash_card(args)
Card *make_splash_card(Table *args, Vec2 *card_pos_out) {
    if (!args) args = tnew();
    num angle = math_random() * 2 * 3.14;
    Val scale = tget(args, "scale");
    num card_size = (truthy(scale) ? vnum(scale) : 1.5) * (math_random() + 1);
    Vec2 card_pos;
    Table *args_card_pos = ttab(args, "card_pos");
    if (args_card_pos) {
        card_pos.x = (float)tnum(args_card_pos, "x");
        card_pos.y = (float)tnum(args_card_pos, "y");
    } else {
        card_pos.x = (float)((18 + card_size) * sin(angle));
        card_pos.y = (float)((18 + card_size) * cos(angle));
    }
    Table *front = pseudorandom_element(G->P_CARDS, NIL, NULL).tab;
    Card *card = card_new(card_pos.x + G->ROOM->T->w / 2 - G->CARD_W * card_size / 2,
                          card_pos.y + G->ROOM->T->h / 2 - G->CARD_H * card_size / 2,
                          card_size * G->CARD_W, card_size * G->CARD_H, front, ttab(G->P_CENTERS, "c_base"));
    if (math_random() > 0.8) { card->sprite_facing = "back"; card->facing = "back"; }
    card->no_shadow = true;
    card->moveable.node.states.hover.can = false;
    card->moveable.node.states.drag.can = false;
    mirror_states(card);
    card->vortex = true && !tbool(args, "no_vortex");
    card->moveable.node.T->r = (float)angle;
    if (card_pos_out) *card_pos_out = card_pos;
    return card;
}

struct splash_whoosh_ctx { Card *card; int temp_i; num temp_pitch; };

// game.lua:1486
static bool ev_splash_vortex_gone(void *ctx) {
    struct splash_whoosh_ctx *c = ctx;
    if (c->card->moveable.node.T->scale <= 0) {
        if (c->temp_i < 30) {
            play_sound("whoosh1", (float)(c->temp_pitch + math_random() * 0.05), (float)(0.25 * (1 - c->temp_i / 50.0)));
        }

        if (c->temp_i == 15) {
            play_sound("whoosh_long", 0.9f, 0.7f);
        }
        G->VIBRATION = G->VIBRATION + 0.1f;
        Card_remove(c->card);
        return true;
    }
    return false;
}

struct splash_vortex_ctx { int i; };

// game.lua:1476
static bool ev_splash_vortex_card(void *ctx) {
    int i = ((struct splash_vortex_ctx *)ctx)->i;
    Vec2 card_pos;
    Card *card = make_splash_card(TAB("scale", N(2 - i / 300.0)), &card_pos);
    num speed = lmax(2. - i * 0.005, 0.001);
    Transform *cT = card->moveable.node.T;
    // note: these eases point into the card, which the event below removes once T.scale <= 0. The scale ease is the
    // longest (1.*speed against 0.9*speed) and reaches 0 on its last pass, queued before that event: none outlives the card.
    ease_value_f(&cT->scale, -cT->scale, false, NULL, -1, (1. * speed), "elastic");
    ease_value_f(&cT->x, -card_pos.x, false, NULL, -1, (0.9 * speed), NULL);
    ease_value_f(&cT->y, -card_pos.y, false, NULL, -1, (0.9 * speed), NULL);
    struct splash_whoosh_ctx *w = NEWCTX(struct splash_whoosh_ctx);
    w->card = card;
    w->temp_pitch = i * 0.007 + 0.6;
    w->temp_i = i;
    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .func = ev_splash_vortex_gone, .ctx = w}), NULL, false);
    return true;
}

// game.lua:1506
static bool ev_splash_to_menu(void *ctx) {
    Sprite_remove(G->SPLASH_BACK);
    G->SPLASH_BACK = G->SPLASH_FRONT;
    G->SPLASH_FRONT = NULL;
    Game_main_menu(G, "splash");
    return true;
}

// game.lua:1391
static bool ev_splash_setup(void *ctx) {
    G->TIMERS.TOTAL = 0;
    G->TIMERS.REAL = 0;
    // Prep the splash screen shaders for both the background (colour swirl) and the foreground (white flash)
    Sprite *back = sprite_new(-30, -13, G->ROOM->T->w + 60, G->ROOM->T->h + 22, ASSET_ATLAS_get("ui_1"), 2, 0);
    G->SPLASH_BACK = back;
    Table *back_send = ARR(
        T(TAB("name", S("time"), "ref_num", P(&G->TIMERS.REAL))),
        T(TAB("name", S("vort_speed"), "val", N(1))),
        T(TAB("name", S("colour_1"), "ref_colour", P(G->C.BLUE))),
        T(TAB("name", S("colour_2"), "ref_colour", P(G->C.WHITE))),
        T(TAB("name", S("mid_flash"), "val", N(0))),
        T(TAB("name", S("vort_offset"), "val", N(lmod(2 * 90.15315131 * os_time(), 100000)))));
    Sprite_define_draw_steps(G->SPLASH_BACK, ARR(T(TAB("shader", S("splash"), "send", T(back_send)))));
    Moveable_set_alignment((Moveable *)G->SPLASH_BACK, cm_alignment(G->ROOM_ATTACH, NULL));
    Sprite *front = sprite_new(0, -20, G->ROOM->T->w * 2, G->ROOM->T->h * 4, ASSET_ATLAS_get("ui_1"), 2, 0);
    G->SPLASH_FRONT = front;
    Table *front_send = ARR(
        T(TAB("name", S("time"), "ref_num", P(&G->TIMERS.REAL))),
        T(TAB("name", S("mid_flash"), "val", N(1))));
    Sprite_define_draw_steps(G->SPLASH_FRONT, ARR(T(TAB("shader", S("flash"), "send", T(front_send)))));
    Moveable_set_alignment((Moveable *)G->SPLASH_FRONT, cm_alignment(G->ROOM_ATTACH, NULL));

    // spawn in splash card
    struct splash_sc_ctx *sc = NEWCTX(struct splash_sc_ctx); // local SC = nil
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_splash_card_in, .ctx = sc}), NULL, false);

    // dissolve fool card and start to fade in the vortex
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 1.8, .func = ev_splash_card_dissolve, .ctx = sc}), NULL, false);

    // create all the cards and suck them in (game.lua:1447: make_splash_card is defined here in the Lua)

    G->vortex_time = G->TIMERS.REAL;
    num temp_del = 0;
    bool temp_del_set = false; // local temp_del = nil

    for (int i = 1; i <= 200; i++) {
        if (!temp_del_set) { temp_del = 3; temp_del_set = true; }
        struct splash_vortex_ctx *v = NEWCTX(struct splash_vortex_ctx);
        v->i = i;
        E_add(G->E_MANAGER, Event_new((EventCfg){
            .trigger = "after",
            .not_blockable = true,
            .delay = temp_del,
            .func = ev_splash_vortex_card, .ctx = v}), NULL, false);
        temp_del = temp_del + lmax(1.0 / i, lmax(0.2 * (170 - i) / 500, 0.016));
    }

    // when faded to white, spit out the 'Fool's' cards and slowly have them settle in to place
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 2., .func = ev_splash_to_menu}), NULL, false);
    return true;
}

// game.lua:1373 Game:splash_screen()
void Game_splash_screen(Game *self) {
    // If the skip splash screen option is set, immediately go to the main menu here
    if (streq(tstr(G->SETTINGS, "skip_splash"), "Yes")) {
        Game_main_menu(G, NULL);
        return;
    }

    Game_prep_stage(self, STAGES_MAIN_MENU, STATES_SPLASH, true);
    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_splash_discover}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_splash_setup}), NULL, false);
}

// ---------------------------------------------------------------- Game:main_menu
// what the events of main_menu / demo_cta capture
struct menu_ctx {
    Game *self;
    const char *change_context;
    Card *replace_card;
    Card *card; // `local card` (game.lua:1648): written by one event, read by the next
    Table *viable_unlockables;
    num SC_scale;
};

// game.lua:1562
static bool ev_menu_unlock_notify(void *ctx) {
    unlock_notify();
    return true;
}

// game.lua:1611
static bool ev_menu_materialize(void *ctx) {
    struct menu_ctx *c = ctx;
    Card *replace_card = c->replace_card;
    if (streq(c->change_context, "splash")) {
        replace_card->moveable.node.states.visible = true;
        Card_start_materialize(replace_card, white_white(), true, 2.5);
        play_sound("whoosh1", (float)(math_random() * 0.1 + 0.3), 0.3f);
        const char *crumple = fmt("crumple%d", (int)math_random_range(1, 5));
        play_sound(crumple, (float)(math_random() * 0.2 + 0.6), 0.65f);
    } else {
        replace_card->moveable.node.states.visible = true;
        Card_start_materialize(replace_card, white_white(), false, 1.2);
    }
    G->VIBRATION = G->VIBRATION + 1;
    return true;
}

// game.lua:1630
static bool ev_menu_logo(void *ctx) {
    struct menu_ctx *c = ctx;
    bool splash = streq(c->change_context, "splash");
    play_sound(fmt("magic_crumple%d", splash ? 2 : 3), splash ? 1 : 1.3f, 0.9f);
    play_sound("whoosh1", 0.4f, 0.8f);
    // G.SPLASH_LOGO.dissolve is in the sprite's dyn (game.h)
    ease_value(G->SPLASH_LOGO->moveable.node.object.dyn, "dissolve", -1, false, NULL, -1, splash ? 2.3 : 0.9, NULL);
    G->VIBRATION = G->VIBRATION + 1.5f;
    return true;
}

// game.lua:1652
static bool ev_menu_teaser_create(void *ctx) {
    struct menu_ctx *c = ctx;
    Game *self = c->self;
    Val center = pseudorandom_element(c->viable_unlockables, NIL, NULL);
    Transform *tT = self->title_top->moveable.node.T;
    Card *card = card_new(tT->x, tT->y, 1.2 * G->CARD_W * c->SC_scale, 1.2 * G->CARD_H * c->SC_scale, NULL,
                          truthy(center) ? center.tab : ttab(self->P_CENTERS, "j_joker"));
    c->card = card;
    card->no_ui = tlen(c->viable_unlockables) == 0;
    card->moveable.node.states.visible = false;
    c->replace_card->moveable.node.parent = NULL;
    Card_start_dissolve(c->replace_card, ARR(P(G->C.BLACK), P(G->C.ORANGE), P(G->C.RED), P(G->C.GOLD)), false, -1, false);
    return true;
}

// game.lua:1663
static bool ev_menu_teaser_show(void *ctx) {
    struct menu_ctx *c = ctx;
    Card_start_materialize(c->card, NULL, false, -1);
    CardArea_emplace(c->self->title_top, c->card, NULL, false);
    return true;
}

// game.lua:1671, 1814
static bool ev_menu_unlock_input(void *ctx) {
    tsetb(G->CONTROLLER->object.dyn, "lock_input", false); // lock_input is in the controller's dyn (controller.h)
    return true;
}

// game.lua:1684
static bool ev_menu_ui(void *ctx) {
    set_main_menu_UI();
    return true;
}

// game.lua:1698
static bool ev_menu_tallies(void *ctx) {
    set_discover_tallies();
    set_profile_progress();
    G->REFRESH_ALERTS = true;
    return true;
}

// game.lua:1537-1557 / 1728-1748: the background sprite of the menu, shared text of main_menu and demo_cta.
// mid_flash: the start of splash_args.mid_flash, eased back to 0 in 4 s.
static void menu_splash_back(num mid_flash, num ease_mod) {
    if (G->SPLASH_FRONT) { Sprite_remove(G->SPLASH_FRONT); G->SPLASH_FRONT = NULL; }
    if (G->SPLASH_BACK) { Sprite_remove(G->SPLASH_BACK); G->SPLASH_BACK = NULL; }
    Sprite *back = sprite_new(-30, -13, G->ROOM->T->w + 60, G->ROOM->T->h + 22, ASSET_ATLAS_get("ui_1"), 2, 0);
    G->SPLASH_BACK = back;
    Moveable_set_alignment((Moveable *)G->SPLASH_BACK, cm_alignment(G->ROOM_ATTACH, NULL));
    Table *splash_args = TAB("mid_flash", N(mid_flash));
    ease_value(splash_args, "mid_flash", ease_mod, false, NULL, -1, 4, NULL);

    Table *send = ARR(
        T(TAB("name", S("time"), "ref_num", P(&G->TIMERS.REAL_SHADER))),
        T(TAB("name", S("vort_speed"), "val", N(0.4))),
        T(TAB("name", S("colour_1"), "ref_colour", P(G->C.RED))),
        T(TAB("name", S("colour_2"), "ref_colour", P(G->C.BLUE))),
        T(TAB("name", S("mid_flash"), "ref_table", T(splash_args), "ref_value", S("mid_flash"))),
        T(TAB("name", S("vort_offset"), "val", N(0))));
    Sprite_define_draw_steps(G->SPLASH_BACK, ARR(T(TAB("shader", S("splash"), "send", T(send)))));
}

// game.lua:1570-1604 / 1752-1785: G.title_top, G.SPLASH_LOGO and the Ace that the logo's card replaces.
static Card *menu_title(Game *self, num SC_scale) {
    // local CAI = {TITLE_TOP_W = G.CARD_W, TITLE_TOP_H = G.CARD_H}
    float TITLE_TOP_W = G->CARD_W, TITLE_TOP_H = G->CARD_H;
    CardArea *title_top = NEW(CardArea);
    CardArea_init(title_top, 0, 0, TITLE_TOP_W, TITLE_TOP_H, TAB("card_limit", N(1), "type", S("title")));
    self->title_top = title_top;

    Atlas *balatro = ASSET_ATLAS_get("balatro");
    Sprite *logo = sprite_new(0, 0, 13 * SC_scale, 13 * SC_scale * (balatro->py / balatro->px), balatro, 0, 0);
    G->SPLASH_LOGO = logo;

    Moveable_set_alignment((Moveable *)G->SPLASH_LOGO, cm_alignment(G->title_top, "Strong"));
    Sprite_define_draw_steps(G->SPLASH_LOGO, ARR(T(TAB("shader", S("dissolve")))));

    // dissolve_colours and dissolve of a plain Sprite are in its dyn (game.h)
    tsett(G->SPLASH_LOGO->moveable.node.object.dyn, "dissolve_colours", white_white());
    tsetn(G->SPLASH_LOGO->moveable.node.object.dyn, "dissolve", 1);

    Transform *tT = self->title_top->moveable.node.T;
    Card *replace_card = card_new(tT->x, tT->y, 1.2 * G->CARD_W * SC_scale, 1.2 * G->CARD_H * SC_scale, ttab(G->P_CARDS, "S_A"),
                                  ttab(G->P_CENTERS, "c_base"));
#ifdef NANDBG
    printf("NANDBG %s: scale %g CARD_W %g tT %g %g %g %g card T %g %g %g %g\n", "new", (double)SC_scale, (double)G->CARD_W, (double)tT->x, (double)tT->y, (double)tT->w, (double)tT->h, (double)replace_card->moveable.node.T->x, (double)replace_card->moveable.node.T->y, (double)replace_card->moveable.node.T->w, (double)replace_card->moveable.node.T->h);
#endif
    CardArea_emplace(self->title_top, replace_card, NULL, false);
#ifdef NANDBG
    printf("NANDBG %s: scale %g CARD_W %g tT %g %g %g %g card T %g %g %g %g\n", "emplaced", (double)SC_scale, (double)G->CARD_W, (double)tT->x, (double)tT->y, (double)tT->w, (double)tT->h, (double)replace_card->moveable.node.T->x, (double)replace_card->moveable.node.T->y, (double)replace_card->moveable.node.T->w, (double)replace_card->moveable.node.T->h);
#endif

    replace_card->moveable.node.states.visible = false;
    replace_card->no_ui = true;
    replace_card->ambient_tilt = 0.0f;
    return replace_card;
}

// game.lua:1517 Game:main_menu(change_context)
void Game_main_menu(Game *self, const char *change_context) {
    bool splash = streq(change_context, "splash"), game = streq(change_context, "game");
    if (!splash) {
        // Skip the timer to 14 seconds for all shaders that need it
        G->TIMERS.REAL = 12;
        G->TIMERS.TOTAL = 12;
    } else {
        // keep all sounds that came from splash screen
        // game.lua:1524 RESET_STATES(G.STATES.MENU): on the PC this is the main thread's copy, which works on lists
        // that stay empty; it must not reach io.h's RESET_STATES (io.h), so nothing is called here.
    }

    // Prepare the main menu, reset the default deck
    Game_prep_stage(self, STAGES_MAIN_MENU, STATES_MENU, true);
    Back *selected_back = NEW(Back);
    Back_init(selected_back, ttab(G->P_CENTERS, "b_red"));
    tseto(self->GAME, "selected_back", selected_back);

    if (!tbool(G->SETTINGS, "tutorial_complete")) {
        Table *completed_parts = ttab(ttab(G->SETTINGS, "tutorial_progress"), "completed_parts");
        // note: the Lua indexes tutorial_progress.completed_parts here and fails if either is nil
        if (!completed_parts) ERROR("game.lua:1531: attempt to index G.SETTINGS.tutorial_progress.completed_parts (a nil value)");
        if (tbool(completed_parts, "big_blind")) tsetb(G->SETTINGS, "tutorial_complete", true);
    }

    FUNCS_change_shadows(TAB("to_key", N(streq(tstr(ttab(G->SETTINGS, "GRAPHICS"), "shadows"), "On") ? 1 : 2)));

    ease_background_colour(TAB("new_colour", P(G->C.BLACK), "contrast", N(1)));

    menu_splash_back(splash ? 1.6 : 0., -(splash ? 1.6 : 0));

    // Display the unlocked decks and cards from the previous run
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_menu_unlock_notify}), NULL, false);

    num SC_scale = 1.1 * (G->debug_splash_size_toggle ? 0.8 : 1);
    Card *replace_card = menu_title(self, SC_scale);

    struct menu_ctx *c = NEWCTX(struct menu_ctx);
    c->self = self;
    c->change_context = str(change_context);
    c->replace_card = replace_card;
    c->SC_scale = SC_scale;

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = game ? 1.5 : 0,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_menu_materialize, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = splash ? 1.8 : game ? 2 : 0.5,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_menu_logo, .ctx = c}), NULL, false);

    delay((0.1 + (splash ? 2 : game ? 1.5 : 0)), NULL);

    if (replace_card && tbool(ttab(G->P_CENTERS, "j_blueprint"), "unlocked")) {
        Table *viable_unlockables = tnew();
        for (int k = 1;; k++) { // ipairs(self.P_LOCKED)
            Val vv = tgeti(self->P_LOCKED, k);
            if (vv.t == V_NIL) break;
            Table *v = vv.tab;
            if ((streq(tstr(v, "set"), "Voucher") || streq(tstr(v, "set"), "Joker")) && !tbool(v, "demo")) {
                tseti(viable_unlockables, tlen(viable_unlockables) + 1, T(v));
            }
        }
        if (tlen(viable_unlockables) > 0) {
            c->viable_unlockables = viable_unlockables;
            E_add(G->E_MANAGER, Event_new((EventCfg){
                .trigger = "after",
                .delay = 4.04,
                .func = ev_menu_teaser_create, .ctx = c}), NULL, false);
            E_add(G->E_MANAGER, Event_new((EventCfg){
                .trigger = "after",
                .delay = 1.04,
                .func = ev_menu_teaser_show, .ctx = c}), NULL, false);
        }
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_menu_unlock_input}), NULL, false);
    set_screen_positions();

    CardArea_sort(self->title_top, "order");
    CardArea_set_ranks(self->title_top);
    CardArea_align_cards(self->title_top);
    CardArea_hard_set_cards(self->title_top);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = splash ? 4.05 : game ? 3 : 1.5,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_menu_ui}), NULL, false);

    // Do all career stat unlock checking here as well
    Table *career_stats = ttab(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "career_stats");
    if (!career_stats) ERROR("game.lua:1691: bad argument #1 to 'pairs' (table expected, got nil)");
    // note: pairs(): the Lua's order is its hash order; here the table's insertion order (only the order of the
    // unlock checks changes)
    for (TIter it = titer(career_stats); tnext(&it);) {
        check_for_unlock(TAB("type", S("career_stat"), "statname", it.key));
    }
    check_for_unlock(TAB("type", S("blind_discoveries")));

    E_add(G->E_MANAGER, Event_new((EventCfg){.not_blockable = true, .func = ev_menu_tallies}), NULL, false);

    // VERSION
    Table *version = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.UI.TRANSPARENT_DARK))), "nodes", T(ARR(
        T(TAB("n", N(UIT_T), "config", T(TAB("text", S(G->VERSION), "scale", N(0.3), "colour", P(G->C.UI.TEXT_LIGHT))))))));
    UIBox_new(TAB(
        "definition", T(version),
        "config", T(TAB("align", S("tri"), "offset", T(TAB("x", N(0), "y", N(0))), "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
}

// ---------------------------------------------------------------- Game:demo_cta
// game.lua:1792
static bool ev_cta_materialize(void *ctx) {
    struct menu_ctx *c = ctx;
    Card *replace_card = c->replace_card;
    replace_card->moveable.node.states.visible = true;
    Card_start_materialize(replace_card, white_white(), true, 2.5);
    play_sound("whoosh1", (float)(math_random() * 0.1 + 0.3), 0.3f);
    const char *crumple = fmt("crumple%d", (int)math_random_range(1, 5));
    play_sound(crumple, (float)(math_random() * 0.2 + 0.6), 0.65f);
    return true;
}

// game.lua:1805
static bool ev_cta_logo(void *ctx) {
    play_sound(fmt("magic_crumple%d", 2), 1, 0.9f);
    play_sound("whoosh1", 0.4f, 0.8f);
    ease_value(G->SPLASH_LOGO->moveable.node.object.dyn, "dissolve", -1, false, NULL, -1, 2.3, NULL);
    return true;
}

// game.lua:1852
static bool ev_cta_ui(void *ctx) {
    Game *self = ((struct menu_ctx *)ctx)->self;
    self->MAIN_MENU_UI->moveable.node.states.visible = true;
    self->MAIN_MENU_UI->moveable.alignment.offset.y = 0;
    Moveable_align_to_major((Moveable *)self->MAIN_MENU_UI);
    Controller_snap_to(G->CONTROLLER, TAB("node", O(UIBox_get_UIE_by_ID(self->MAIN_MENU_UI, S("demo_cta_playbalatro"), NULL))));
    return true;
}

// game.lua:1716 Game:demo_cta()
void Game_demo_cta(Game *self) {
    // Prepare the main menu, reset the default deck
    Game_prep_stage(self, STAGES_MAIN_MENU, STATES_DEMO_CTA, true);

    Back *selected_back = NEW(Back);
    Back_init(selected_back, ttab(G->P_CENTERS, "b_red"));
    tseto(self->GAME, "selected_back", selected_back);

    FUNCS_change_shadows(TAB("to_key", N(streq(tstr(ttab(G->SETTINGS, "GRAPHICS"), "shadows"), "On") ? 1 : 2)));

    ease_background_colour(TAB("new_colour", P(G->C.BLACK), "contrast", N(1)));

    menu_splash_back(1.6, -1.6);

    num SC_scale = 0.9 * (G->debug_splash_size_toggle ? 0.8 : 1);

    Card *replace_card = menu_title(self, SC_scale);

    struct menu_ctx *c = NEWCTX(struct menu_ctx);
    c->self = self;
    c->replace_card = replace_card;
    c->SC_scale = SC_scale;

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 1.1,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_cta_materialize, .ctx = c}), NULL, false);

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 1.8,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_cta_logo}), NULL, false);

    delay((0.1 + 2), NULL);

    E_add(G->E_MANAGER, Event_new((EventCfg){.func = ev_menu_unlock_input}), NULL, false);
    set_screen_positions();

    CardArea_sort(self->title_top, "order");
    CardArea_set_ranks(self->title_top);
    CardArea_align_cards(self->title_top);
    CardArea_hard_set_cards(self->title_top);

    Sprite *playstack = sprite_new(0, 0, 1.7f, 1.7f, ASSET_ATLAS_get("playstack_logo"), 0, 0);
    playstack->moveable.node.states.drag.can = false;
    Sprite *localthunk = sprite_new(0, 0, 1 * 1390 / 560.0, 1, ASSET_ATLAS_get("localthunk_logo"), 0, 0);
    localthunk->moveable.node.states.drag.can = false;

    // note: the objects of the definition are made first, in the order the Lua's table constructor makes them
    DynaText *text = DynaText_new(TAB("string", T(ARR(S(lit("demo_cta", 1)))), "colours", T(ARR(P(G->C.WHITE))),
                                      "shadow", B(true), "rotate", B(true), "float", B(true), "bump", B(true), "scale", N(0.9),
                                      "spacing", N(1), "pop_in", N(4.5)));
    Table *back_button = UIBox_button(TAB("button", S("go_to_menu"), "colour", P(G->C.ORANGE), "minw", N(2), "minh", N(1),
                                          "label", T(ARR(S(lit("demo_cta", 2)))), "scale", N(0.4), "col", B(true)));
    Table *play_button = UIBox_button(TAB("id", S("demo_cta_playbalatro"), "button", S("go_to_playbalatro"), "colour", P(G->C.BLUE),
                                          "minw", N(7.65), "minh", N(1.95), "label", T(ARR(S(lit("demo_cta", 3)))), "scale", N(0.9),
                                          "col", B(true)));
    Table *row_1 = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.3))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(text))))))));
    Table *col_l = TAB("n", N(UIT_C), "config", T(TAB("align", S("cl"), "minw", N(5), "minh", N(1))), "nodes", T(ARR(
        T(back_button))));
    Table *col_r = TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "minw", N(5), "minh", N(1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(localthunk))))),
        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(playstack))))))));
    Table *row_2 = TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.3))), "nodes", T(ARR(
        T(col_l),
        T(play_button),
        T(col_r))));
    Table *definition = TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
        T(row_1),
        T(row_2))));
    UIBox *ui = UIBox_new(TAB(
        "definition", T(definition),
        "config", T(TAB("align", S("bmi"), "offset", T(TAB("x", N(0), "y", N(10))), "major", O(G->ROOM_ATTACH), "bond", S("Weak")))));
    self->MAIN_MENU_UI = ui;
    self->MAIN_MENU_UI->moveable.node.states.visible = false;

    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 4.05,
        .not_blockable = true,
        .not_blocking = true,
        .func = ev_cta_ui, .ctx = c}), NULL, false);
}

// ---------------------------------------------------------------- Game:init_game_object
// A string of the GAME section of data/game.dat (data.h: what the Lua's Game:init_game_object returns): the game's
// own wording is taken from there, not typed here.
static Val game_text(const char *path) {
    Val v = tpath(ttab(game_dat(), "GAME"), path);
    if (v.t != V_STR) ERROR("data/game.dat: GAME.%s is not a string: regenerate it with tools/dump_data.sh", path);
    return v;
}

// round_scores[key] = {label = <the Lua's label>, amt = 0}
static Val score_row(const char *key) { return T(TAB("label", game_text(fmt("round_scores.%s.label", key)), "amt", N(0))); }

// {suits = {}, total = 0}
static Val played_row(void) { return T(TAB("suits", T(tnew()), "total", N(0))); }

// {'S_A', true}
static Val ex(const char *card, bool scoring) { return T(ARR(S(card), B(scoring))); }

// One row of the hands table (game.lua:2002-2013), the Lua's keys in the Lua's order.
static Val hand_row(bool visible, num order, num mult, num chips, num s_mult, num s_chips, num level, num l_mult, num l_chips,
                    num played, num played_this_round, Table *example) {
    return T(TAB("visible", B(visible), "order", N(order), "mult", N(mult), "chips", N(chips), "s_mult", N(s_mult),
                 "s_chips", N(s_chips), "level", N(level), "l_mult", N(l_mult), "l_chips", N(l_chips), "played", N(played),
                 "played_this_round", N(played_this_round), "example", T(example)));
}

// game.lua:1862 Game:init_game_object()
Table *Game_init_game_object(Game *self) {
    Table *bosses_used = tnew();
    for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
        if (tbool(it.val.tab, "boss")) tsetv(bosses_used, it.key, N(0));
    }
    // (the keys the Lua sets to nil are absent: last_tarot_planet, blind, blind_on_deck, round_resets.temp_reroll_cost,
    // round_resets.temp_handsize)
    Table *round_scores = TAB(
        "furthest_ante", score_row("furthest_ante"),
        "furthest_round", score_row("furthest_round"),
        "hand", score_row("hand"),
        "poker_hand", score_row("poker_hand"),
        "new_collection", score_row("new_collection"),
        "cards_played", score_row("cards_played"),
        "cards_discarded", score_row("cards_discarded"),
        "times_rerolled", score_row("times_rerolled"),
        "cards_purchased", score_row("cards_purchased"));
    Table *current_round = TAB(
        "current_hand", T(TAB(
            "chips", N(0),
            "chip_text", S("0"),
            "mult", N(0),
            "mult_text", S("0"),
            "chip_total", N(0),
            "chip_total_text", S(""),
            "handname", S(""),
            "hand_level", S(""))),
        "used_packs", T(tnew()),
        "cards_flipped", N(0),
        "round_text", game_text("current_round.round_text"),
        "idol_card", T(TAB("suit", S("Spades"), "rank", S("Ace"))),
        "mail_card", T(TAB("rank", S("Ace"))),
        "ancient_card", T(TAB("suit", S("Spades"))),
        "castle_card", T(TAB("suit", S("Spades"))),
        "hands_left", N(0),
        "hands_played", N(0),
        "discards_left", N(0),
        "discards_used", N(0),
        "dollars", N(0),
        "reroll_cost", N(5),
        "reroll_cost_increase", N(0),
        "jokers_purchased", N(0),
        "free_rerolls", N(0),
        "round_dollars", N(0),
        "dollars_to_be_earned", S("!!!"),
        "most_played_poker_hand", S("High Card"));
    Table *round_resets = TAB(
        "hands", N(1),
        "discards", N(1),
        "reroll_cost", N(1),
        "ante", N(1),
        "blind_ante", N(1),
        "blind_states", T(TAB("Small", S("Select"), "Big", S("Upcoming"), "Boss", S("Upcoming"))),
        "loc_blind_states", T(TAB("Small", S(""), "Big", S(""), "Boss", S(""))),
        "blind_choices", T(TAB("Small", S("bl_small"), "Big", S("bl_big"))),
        "boss_rerolled", B(false));
    Table *cards_played = TAB(
        "Ace", played_row(),
        "2", played_row(),
        "3", played_row(),
        "4", played_row(),
        "5", played_row(),
        "6", played_row(),
        "7", played_row(),
        "8", played_row(),
        "9", played_row(),
        "10", played_row(),
        "Jack", played_row(),
        "Queen", played_row(),
        "King", played_row());
    Table *hands = TAB(
        "Flush Five",      hand_row(false, 1, 16, 160, 16, 160, 1, 3, 50, 0, 0, ARR(ex("S_A", true), ex("S_A", true), ex("S_A", true), ex("S_A", true), ex("S_A", true))),
        "Flush House",     hand_row(false, 2, 14, 140, 14, 140, 1, 4, 40, 0, 0, ARR(ex("D_7", true), ex("D_7", true), ex("D_7", true), ex("D_4", true), ex("D_4", true))),
        "Five of a Kind",  hand_row(false, 3, 12, 120, 12, 120, 1, 3, 35, 0, 0, ARR(ex("S_A", true), ex("H_A", true), ex("H_A", true), ex("C_A", true), ex("D_A", true))),
        "Straight Flush",  hand_row(true, 4, 8, 100, 8, 100, 1, 4, 40, 0, 0, ARR(ex("S_Q", true), ex("S_J", true), ex("S_T", true), ex("S_9", true), ex("S_8", true))),
        "Four of a Kind",  hand_row(true, 5, 7, 60, 7, 60, 1, 3, 30, 0, 0, ARR(ex("S_J", true), ex("H_J", true), ex("C_J", true), ex("D_J", true), ex("C_3", false))),
        "Full House",      hand_row(true, 6, 4, 40, 4, 40, 1, 2, 25, 0, 0, ARR(ex("H_K", true), ex("C_K", true), ex("D_K", true), ex("S_2", true), ex("D_2", true))),
        "Flush",           hand_row(true, 7, 4, 35, 4, 35, 1, 2, 15, 0, 0, ARR(ex("H_A", true), ex("H_K", true), ex("H_T", true), ex("H_5", true), ex("H_4", true))),
        "Straight",        hand_row(true, 8, 4, 30, 4, 30, 1, 3, 30, 0, 0, ARR(ex("D_J", true), ex("C_T", true), ex("C_9", true), ex("S_8", true), ex("H_7", true))),
        "Three of a Kind", hand_row(true, 9, 3, 30, 3, 30, 1, 2, 20, 0, 0, ARR(ex("S_T", true), ex("C_T", true), ex("D_T", true), ex("H_6", false), ex("D_5", false))),
        "Two Pair",        hand_row(true, 10, 2, 20, 2, 20, 1, 1, 20, 0, 0, ARR(ex("H_A", true), ex("D_A", true), ex("C_Q", false), ex("H_4", true), ex("C_4", true))),
        "Pair",            hand_row(true, 11, 2, 10, 2, 10, 1, 1, 15, 0, 0, ARR(ex("S_K", false), ex("S_9", true), ex("D_9", true), ex("H_6", false), ex("D_3", false))),
        "High Card",       hand_row(true, 12, 1, 5, 1, 5, 1, 1, 10, 0, 0, ARR(ex("S_A", true), ex("D_Q", false), ex("D_9", false), ex("C_4", false), ex("D_3", false))));
    return TAB(
        "won", B(false),
        "round_scores", T(round_scores),
        "joker_usage", T(tnew()),
        "consumeable_usage", T(tnew()),
        "hand_usage", T(tnew()),
        "win_ante", N(8),
        "stake", N(1),
        "modifiers", T(tnew()),
        "starting_params", T(get_starting_params()),
        "banned_keys", T(tnew()),
        "round", N(0),
        "probabilities", T(TAB(
            "normal", N(1))),
        "bosses_used", T(bosses_used),
        "pseudorandom", T(tnew()),
        "starting_deck_size", N(52),
        "ecto_minus", N(1),
        "pack_size", N(2),
        "skips", N(0),
        "STOP_USE", N(0),
        "edition_rate", N(1),
        "joker_rate", N(20),
        "tarot_rate", N(4),
        "planet_rate", N(4),
        "spectral_rate", N(0),
        "playing_card_rate", N(0),
        "consumeable_buffer", N(0),
        "joker_buffer", N(0),
        "discount_percent", N(0),
        "interest_cap", N(25),
        "interest_amount", N(1),
        "inflation", N(0),
        "hands_played", N(0),
        "unused_discards", N(0),
        "perishable_rounds", N(5),
        "rental_rate", N(3),
        "chips", N(0),
        "chips_text", S("0"),
        "voucher_text", S(""),
        "dollars", N(0),
        "max_jokers", N(0),
        "bankrupt_at", N(0),
        "current_boss_streak", N(0),
        "base_reroll_cost", N(5),
        "sort", S("desc"),
        "previous_round", T(TAB(
            "dollars", N(4))),
        "tags", T(tnew()),
        "tag_tally", N(0),
        "pool_flags", T(tnew()),
        "used_jokers", T(tnew()),
        "used_vouchers", T(tnew()),
        "current_round", T(current_round),
        "round_resets", T(round_resets),
        "round_bonus", T(TAB(
            "next_hands", N(0),
            "discards", N(0))),
        "shop", T(TAB(
            "joker_max", N(2))),
        "cards_played", T(cards_played),
        "hands", T(hands));
}
