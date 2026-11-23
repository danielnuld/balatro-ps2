// functions/button_callbacks.lua:3040-3203: show_main_cta and the screen wipe (wipe_on, wipe_off).
#include "all.h"

// button_callbacks.lua:3040
void FUNCS_show_main_cta(UIElement *e) {
    if (e) {
        Table *config = e->moveable.node.config;
        Table *DEMO = ttab(G->SETTINGS, "DEMO");
        if (veq(tget(config, "id"), S("lose_cta")) && !tbool(DEMO, "lose_CTA_shown")) {
            tsetb(DEMO, "lose_CTA_shown", true);
        }
        if (veq(tget(config, "id"), S("win_cta")) && !tbool(DEMO, "win_CTA_shown")) {
            tsetb(DEMO, "win_CTA_shown", true);
        }
    }

    Game_save_progress(G);

    tsetb(G->SETTINGS, "paused", true);
    G->normal_music_speed = true;

    FUNCS_overlay_menu(TAB(
        "definition", T(create_UIBox_demo_video_CTA()),
        "config", T(TAB("no_esc", B(true)))));
}

struct wipe_on_ctx { bool no_card; };

// button_callbacks.lua:3119
static bool ev_wipe_on_flip(void *ctx) {
    struct wipe_on_ctx *c = ctx;
    if (!c->no_card) {
        Card_flip(G->screenwipecard);
        play_sound("cardFan2", -1, -1);
    }
    return true;
}

// button_callbacks.lua:3061
void FUNCS_wipe_on(Table *message, bool no_card, num timefac, const float *alt_colour) {
    if (timefac < 0) timefac = 1;
    if (G->screenwipe) return;
    tsetb(G->CONTROLLER->locks, "wipe", true);
    G->STAGE_OBJECT_INTERRUPT = true;
    // note: {1, 1, 1, 1} is a fresh colour whose alpha wipe_off eases: its own 4 floats, like HEX's
    float *white = malloc(4 * sizeof(float));
    white[0] = 1, white[1] = 1, white[2] = 1, white[3] = 1;
    Table *colours = TAB(
        "black", P(HEX("4f6367FF")),
        "white", P(white));
    if (!no_card) {
        G->screenwipecard = NEW(Card);
        // pseudorandom_element with no seed: the global math.random, as the Lua
        Card_init(G->screenwipecard, 1, 1, G->CARD_W, G->CARD_H, pseudorandom_element(G->P_CARDS, NIL, NULL).tab, ttab(G->P_CENTERS, "c_base"), NULL);
        G->screenwipecard->sprite_facing = "back";
        G->screenwipecard->facing = "back";
        // note: the Lua shares this states table with the card's sprites (card.h, Card_set_sprites); only the
        // card's own States is written, as the card's own files do
        G->screenwipecard->moveable.node.states.hover.can = false;
        Card_juice_up(G->screenwipecard, 0.5f, N(1));
    }
    Table *message_t = NULL;
    if (message) {
        message_t = tnew();
        for (int k = 1; tgeti(message, k).t != V_NIL; k++) {
            Val v = tgeti(message, k);
            tinsert(message_t, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(DynaText_new(TAB(
                    "string", truthy(v) ? v : S(""),
                    "colours", T(ARR(P(fminf(G->C.BACKGROUND.C[0], G->C.BACKGROUND.C[1]) > 0.5f ? G->C.BLACK : G->C.WHITE))),
                    "shadow", B(true), "silent", B(k != 1), "float", B(true), "scale", N(1.3), "pop_in", N(0),
                    "pop_in_rate", N(2), "rotate", N(1)))))))))))));
        }
    }

    // note: the inner row's nodes keep the Lua's indices ({nil, card} when there is no message): ui.h walks
    // 'nodes' with titer, holes skipped
    Table *nodes = tnew();
    if (message) tseti(nodes, 1, T(TAB("n", N(UIT_R), "config", T(TAB("id", S("text"), "align", S("cm"), "padding", N(0.7))), "nodes", T(message_t))));
    if (!no_card) tseti(nodes, 2, T(TAB("n", N(UIT_O), "config", T(TAB("object", O(G->screenwipecard), "role", T(TAB("role_type", S("Major"))))))));
    G->screenwipe = UIBox_new(TAB(
        "definition",
            T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "minw", N(0), "minh", N(0), "padding", N(0.15), "r", N(0.1), "colour", P(G->C.CLEAR))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(nodes)))
            )))),
        "config", T(TAB("align", S("cm"), "offset", T(TAB("x", N(0), "y", N(0))), "major", O(G->ROOM_ATTACH)))));
    Node *wipe = &G->screenwipe->moveable.node;
    // G.screenwipe.colours: in the box's dyn (button_callbacks.h)
    tsett(wipe->object.dyn, "colours", colours);
    Particles *particles = NEW(Particles);
    Particles_init(particles, 0, 0, 0, 0, TAB(
        "timer", N(0),
        "max", N(1),
        "scale", N(40),
        "speed", N(0),
        "lifespan", N(1.7 * timefac),
        "attach", O(G->screenwipe),
        "colours", T(ARR(P(alt_colour ? (float *)alt_colour : G->C.BACKGROUND.C)))));
    tseto(wipe->children, "particles", particles);
    G->STAGE_OBJECT_INTERRUPT = false;
    G->screenwipe->moveable.alignment.offset.y = 0;
    if (message) {
        Table *children = UIBox_get_UIE_by_ID(G->screenwipe, S("text"), NULL)->moveable.node.children;
        for (int k = 1; tgeti(children, k).t != V_NIL; k++) {
            Node *v = tobji(children, k);
            DynaText_pulse(tobj(((Node *)tobji(v->children, 1))->config, "object"), -1);
        }
    }

    struct wipe_on_ctx *c = NEWCTX(struct wipe_on_ctx);
    c->no_card = no_card;
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "before",
        .delay = 0.7,
        .no_delete = true,
        .not_blockable = true,
        .func = ev_wipe_on_flip, .ctx = c}), NULL, false);
}

// button_callbacks.lua:3132
static bool ev_wipe_off_1(void *ctx) {
    delay(0.3, NULL);
    // note: with G.screenwipe nil (wipe_on returned early and a second wipe_off was queued) the Lua errors here and
    // at :3170 and :3185; the spec (button-callbacks.md, trap 40) asks for these events to be nil-safe
    if (!G->screenwipe) return true;
    Node *wipe = &G->screenwipe->moveable.node;
    ((Particles *)tobj(wipe->children, "particles"))->max = 0;
    Table *colours = ttab(wipe->object.dyn, "colours");
    // ref_table = colours.black, ref_value = 4: the colour's alpha (event.h: colour[4] is &colour[3]); the
    // colours are never freed, so the pointers outlive the box
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .no_delete = true,
        .not_blockable = true,
        .not_blocking = true,
        .timer = "REAL",
        .ref_float = &UI_colour(colours, "black")[3],
        .ease_to = 0,
        .delay = 0.3}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "ease",
        .no_delete = true,
        .not_blockable = true,
        .not_blocking = true,
        .timer = "REAL",
        .ref_float = &UI_colour(colours, "white")[3],
        .ease_to = 0,
        .delay = 0.3}), NULL, false);
    return true;
}

// button_callbacks.lua:3168
static bool ev_wipe_off_2(void *ctx) {
    if (G->screenwipecard) Card_start_dissolve(G->screenwipecard, ARR(P(G->C.BLACK), P(G->C.ORANGE), P(G->C.GOLD), P(G->C.RED)), false, -1, false);
    if (!G->screenwipe) return true; // note: nil-safe, see ev_wipe_off_1
    if (UIBox_get_UIE_by_ID(G->screenwipe, S("text"), NULL)) {
        Table *children = UIBox_get_UIE_by_ID(G->screenwipe, S("text"), NULL)->moveable.node.children;
        for (int k = 1; tgeti(children, k).t != V_NIL; k++) {
            Node *v = tobji(children, k);
            DynaText_pop_out(tobj(((Node *)tobji(v->children, 1))->config, "object"), 4);
        }
    }
    return true;
}

// button_callbacks.lua:3184
static bool ev_wipe_off_3(void *ctx) {
    if (G->screenwipe) { // note: nil-safe, see ev_wipe_off_1
        Node *wipe = &G->screenwipe->moveable.node;
        Particles_remove(tobj(wipe->children, "particles"));
        UIBox_remove(G->screenwipe);
        tset(wipe->children, "particles", NIL);
    }
    G->screenwipe = NULL;
    G->screenwipecard = NULL;
    return true;
}

// button_callbacks.lua:3199
static bool ev_wipe_off_4(void *ctx) {
    return true;
}

// button_callbacks.lua:3129
void FUNCS_wipe_off(void) {
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .no_delete = true,
        .func = ev_wipe_off_1}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 0.55,
        .no_delete = true,
        .not_blocking = true,
        .timer = "REAL",
        .func = ev_wipe_off_2}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 1.1,
        .no_delete = true,
        .not_blocking = true,
        .timer = "REAL",
        .func = ev_wipe_off_3}), NULL, false);
    E_add(G->E_MANAGER, Event_new((EventCfg){
        .trigger = "after",
        .delay = 1.2,
        .no_delete = true,
        .timer = "REAL",
        .func = ev_wipe_off_4}), NULL, false);
}
