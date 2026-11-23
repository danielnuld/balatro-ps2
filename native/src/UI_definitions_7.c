// functions/UI_definitions.lua, lines 3809-4327.
// note: C does not fix the evaluation order of function arguments, so every object a definition builds inline in
// the Lua (DynaText, Sprite, UIBox, Moveable: they take IDs and join G.I in creation order) is made in a local
// first, in the Lua's order, and the definition then refers to it.
#include "all.h"

#define NODE(o) ((Node *)(o))

// UI_definitions.lua:3809
Table *create_UIBox_your_collection_enhancements(const char *exit) {
    Table *deck_tables = tnew();

    Table *your_collection = tnew();
    G->your_collection = T(your_collection);
    for (int j = 1; j <= 2; j++) {
        CardArea *area = NEW(CardArea);
        CardArea_init(area, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h,
                      4.25f * G->CARD_W,
                      1.03f * G->CARD_H,
                      TAB("card_limit", N(4), "type", S("title"), "highlight_limit", N(0)));
        tseti(your_collection, j, O(area));
        tinsert(deck_tables,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))));
    }

    for (int i = 1; i <= 4; i++) {
        for (int j = 1; j <= tlen(your_collection); j++) {
            CardArea *area = tobji(your_collection, j);
            Table *center = ttabi(ttab(G->P_CENTER_POOLS, "Enhanced"), i + (j - 1) * 4);
            Card *card = NEW(Card);
            // G.P_CARDS.empty is nil: NULL
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
            CardArea_emplace(area, card, NULL, false);
        }
    }

    Table *t = create_UIBox_generic_options(TAB("infotip", localize(S("ml_edition_seal_enhancement_explanation"), NULL), "back_func", S(exit ? exit : "your_collection"), "snap_back", B(true), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05))), "nodes", T(deck_tables)))
    ))));
    return t;
}

// UI_definitions.lua:3840
Table *create_UIBox_your_collection_editions(void) {
    Table *your_collection = tnew();
    G->your_collection = T(your_collection);
    CardArea *area = NEW(CardArea);
    CardArea_init(area, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h,
                  5.3f * G->CARD_W,
                  1.03f * G->CARD_H,
                  TAB("card_limit", N(5), "type", S("title"), "highlight_limit", N(0), "collection", B(true)));
    tseti(your_collection, 1, O(area));
    Table *deck_tables =
        TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "no_fill", B(true))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
        )));

    static const char *const editions[5] = {"base", "foil", "holo", "polychrome", "negative"};

    for (int i = 1; i <= 5; i++) {
        Table *center = ttab(G->P_CENTERS, fmt("e_%s", editions[i - 1]));
        Card *card = NEW(Card);
        Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H, NULL, center, NULL);
        Card_start_materialize(card, NULL, false, -1);
        if (tbool(center, "discovered")) Card_set_edition(card, TAB(editions[i - 1], B(true)), true, true);
        CardArea_emplace(area, card, NULL, false);
    }

    INIT_COLLECTION_CARD_ALERTS();

    Table *t = create_UIBox_generic_options(TAB("infotip", localize(S("ml_edition_seal_enhancement_explanation"), NULL), "back_func", S("your_collection"), "snap_back", B(true), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(T(deck_tables)))))
    ))));
    return t;
}

// UI_definitions.lua:3869
Table *create_UIBox_your_collection_decks(void) {
    Back *viewed_back = NEW(Back);
    Back_init(viewed_back, ttab(G->P_CENTERS, "b_red"));
    tseto(G->GAME, "viewed_back", viewed_back);

    CardArea *area = NEW(CardArea);
    CardArea_init(area, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h,
                  1.2f * G->CARD_W,
                  1.2f * G->CARD_H,
                  TAB("card_limit", N(52), "type", S("deck"), "highlight_limit", N(0)));

    for (int i = 1; i <= 52; i++) {
        Card *card = NEW(Card);
        Card_init(card, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h, G->CARD_W * 1.2f, G->CARD_H * 1.2f,
                  pseudorandom_element(G->P_CARDS, NIL, NULL).tab, ttab(G->P_CENTERS, "c_base"),
                  TAB("playing_card", N(i), "viewed_back", B(true)));
        card->sprite_facing = "back";
        card->facing = "back";
        CardArea_emplace(area, card, NULL, false);
        if (i == 52) {
            G->sticker_card = card;
            card->sticker = get_deck_win_sticker(ttab(viewed_back->effect, "center"));
        }
    }

    Table *ordered_names = tnew();
    Table *backs = ttab(G->P_CENTER_POOLS, "Back");
    for (int k = 1; ttabi(backs, k); k++) {
        tseti(ordered_names, tlen(ordered_names) + 1, tget(ttabi(backs, k), "name"));
    }

    Moveable *name_object = NEW(Moveable);
    Moveable_init_args(name_object, NULL);
    UIBox *back_box = UIBox_new(TAB("definition", T(Back_generate_UI(viewed_back, NULL, -1, -1, NULL)), "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0)))))));

    Table *mid =
        TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "minw", N(2.5), "padding", N(0.1), "r", N(0.1), "colour", P(G->C.BLACK), "emboss", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.2), "colour", P(G->C.BLACK), "r", N(0.2))), "nodes", T(ARR(
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
                )))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("tm"), "minw", N(3.7), "minh", N(2.1), "r", N(0.1), "colour", P(G->C.L_BLACK), "padding", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "emboss", N(0.1), "r", N(0.1), "minw", N(4), "maxw", N(4), "minh", N(0.6))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("func", S("RUN_SETUP_check_back_name"), "object", O(name_object)))))
                    )))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.WHITE), "emboss", N(0.1), "minh", N(2.2), "r", N(0.1))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("id", S(viewed_back->name), "func", S("RUN_SETUP_check_back"), "object", O(back_box)))))
                    ))))
                ))))
            ))))
        )));
    Table *cycle = create_option_cycle(TAB("options", T(ordered_names), "opt_callback", S("change_viewed_back"), "current_option", N(1), "colour", P(G->C.RED), "w", N(4.5), "focus_args", T(TAB("snap_to", B(true))), "mid", T(mid)));

    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(cycle)
    ))));
    return t;
}

// UI_definitions.lua:3921, :3976 table.sort(t, function (a, b) return a.order < b.order end)
static bool less_by_order(Val a, Val b, void *ctx) {
    return tnum(a.tab, "order") < tnum(b.tab, "order");
}

// UI_definitions.lua:3939, :4032 hangs the alert box on each sprite of the list (ctx: array of O(Sprite))
static bool ev_alert_sprites(void *ctx) {
    Table *to_be_alerted = ctx;
    for (int i = 1; tobji(to_be_alerted, i); i++) {
        Node *v = tobji(to_be_alerted, i);
        UIBox *alert = UIBox_new(TAB(
            "definition", T(create_UIBox_card_alert(NULL)),
            "config", T(TAB("align", S("tri"), "offset", T(TAB("x", N(0.1), "y", N(0.1))), "parent", O(v)))));
        tseto(v->children, "alert", alert);
        alert->moveable.node.states.collide.can = false;
    }
    return true;
}

// UI_definitions.lua:3912
Table *create_UIBox_your_collection_tags(void) {
    Table *tag_matrix = ARR(T(tnew()), T(tnew()), T(tnew()), T(tnew()));
    Table *tag_tab = tnew();
    // note: pairs(G.P_TAGS) is insertion order here; the sort below orders it (the orders are distinct)
    for (TIter it = titer(G->P_TAGS); tnext(&it);) {
        tseti(tag_tab, tlen(tag_tab) + 1, it.val);
    }

    tsort(tag_tab, less_by_order, NULL);

    Table *tags_to_be_alerted = tnew();
    for (int k = 1; ttabi(tag_tab, k); k++) {
        Table *v = ttabi(tag_tab, k);
        Val discovered = tget(v, "discovered");
        Tag *temp_tag = NEW(Tag);
        Tag_init(temp_tag, tstr(v, "key"), true, NULL);
        if (!tbool(v, "discovered")) temp_tag->hide_ability = true;
        Sprite *temp_tag_sprite = NULL;
        Table *temp_tag_ui = Tag_generate_UI(temp_tag, -1, &temp_tag_sprite);
        Table *row = ttabi(tag_matrix, (int)ceil((k - 1) / 6.0 + 0.001));
        if (!row) ERROR("UI_definitions.lua:3929 attempt to index a nil row of tag_matrix (more than 24 tags)");
        tseti(row, 1 + ((k - 1) % 6), T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(temp_tag_ui)
        )))));
        if (truthy(discovered) && !tbool(v, "alerted")) {
            tseti(tags_to_be_alerted, tlen(tags_to_be_alerted) + 1, O(temp_tag_sprite));
        }
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_alert_sprites, .ctx = tags_to_be_alerted}), NULL, false);

    Table *t = create_UIBox_generic_options(TAB("back_func", S("your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.BLACK), "padding", N(0.1), "emboss", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(tag_matrix, 1)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(tag_matrix, 2)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(tag_matrix, 3)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(tag_matrix, 4))))
                ))))
            ))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:4003 and :5834 temp_blind.hover (its upvalues v and discovered are read back from the sprite)
void UIDEF_blind_sprite_hover(AnimatedSprite *temp_blind) {
    Node *node = NODE(temp_blind);
    Table *dyn = node->object.dyn;
    Table *v = ttab(node->config, "blind");
    bool discovered = tbool(dyn, "discovered");
    if (!G->CONTROLLER->dragging.target || tbool(G->CONTROLLER->object.dyn, "using_touch")) {
        if (!tbool(dyn, "hovering") && node->states.visible) {
            tsetb(dyn, "hovering", true);
            tsetn(dyn, "hover_tilt", 3);
            CALL(temp_blind, juice_up, 0.05f, N(0.02));
            play_sound("chips1", (float)(math_random() * 0.1 + 0.55), 0.12f);
            tsett(node->config, "h_popup", create_UIBox_blind_popup(v, discovered, NULL));
            tsett(node->config, "h_popup_config", TAB("align", S("cl"), "offset", T(TAB("x", N(-0.1), "y", N(0))), "parent", O(temp_blind)));
            Node_hover(node);
            void *alert = tobj(node->children, "alert");
            if (alert) {
                CALL(alert, remove);
                tset(node->children, "alert", NIL);
                tsetb(ttab(node->config, "blind"), "alerted", true);
                Game_save_progress(G);
            }
        }
    }
    // UI_definitions.lua:4021 the Lua assigns temp_blind.stop_hover here, inside hover (a misplaced `end`).
    // note: the class has it from the start; before the first hover the Lua's stop_hover is Node's, which leaves
    // hovering and hover_tilt nil where this one writes false and 0.
}

// UI_definitions.lua:4021 and :5847 temp_blind.stop_hover
void UIDEF_blind_sprite_stop_hover(AnimatedSprite *temp_blind) {
    tsetb(NODE(temp_blind)->object.dyn, "hovering", false);
    Node_stop_hover(NODE(temp_blind));
    tsetn(NODE(temp_blind)->object.dyn, "hover_tilt", 0);
}

static void cls_blind_sprite_hover(void *self) { UIDEF_blind_sprite_hover(self); }
static void cls_blind_sprite_stop_hover(void *self) { UIDEF_blind_sprite_stop_hover(self); }
// The class of a blind chip whose hover / stop_hover the Lua replaces (UI_definitions.h)
Class UIDEF_blind_sprite_class = {
    .name = "AnimatedSprite",
    .super = &AnimatedSprite_class,
    .hover = cls_blind_sprite_hover,
    .stop_hover = cls_blind_sprite_stop_hover,
};

// UI_definitions.lua:3989
static bool ev_snap_to_blind(void *ctx) {
    Controller_snap_to(G->CONTROLLER, TAB("node", O(ctx)));
    return true;
}

// UI_definitions.lua:3967
Table *create_UIBox_your_collection_blinds(const char *exit) {
    Table *blind_matrix = ARR(T(tnew()), T(tnew()), T(tnew()), T(tnew()), T(tnew()), T(tnew()));
    Table *blind_tab = tnew();
    // note: pairs(G.P_BLINDS) is insertion order here; the sort below orders it (the orders are distinct)
    for (TIter it = titer(G->P_BLINDS); tnext(&it);) {
        tseti(blind_tab, tlen(blind_tab) + 1, it.val);
    }

    tsort(blind_tab, less_by_order, NULL);

    Table *blinds_to_be_alerted = tnew();
    for (int k = 1; ttabi(blind_tab, k); k++) {
        Table *v = ttabi(blind_tab, k);
        Val discovered = tget(v, "discovered");
        AnimatedSprite *temp_blind = NEW(AnimatedSprite);
        AnimatedSprite_init(temp_blind, 0, 0, 1.3f, 1.3f, ANIMATION_ATLAS_get("blind_chips"),
                            (truthy(discovered) && ttab(v, "pos")) ? ttab(v, "pos") : ttab(G->b_undiscovered, "pos"));
        Sprite_define_draw_steps((Sprite *)temp_blind, ARR(
            T(TAB("shader", S("dissolve"), "shadow_height", N(0.05))),
            T(TAB("shader", S("dissolve")))
        ));
        if (k == 1) {
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_snap_to_blind, .ctx = temp_blind}), NULL, false);
        }
        tsetb(NODE(temp_blind)->object.dyn, "float", true);
        NODE(temp_blind)->states.hover.can = true;
        NODE(temp_blind)->states.drag.can = false;
        NODE(temp_blind)->states.collide.can = true;
        NODE(temp_blind)->config = TAB("blind", T(v), "force_focus", B(true));
        if (truthy(discovered) && !tbool(v, "alerted")) {
            tseti(blinds_to_be_alerted, tlen(blinds_to_be_alerted) + 1, O(temp_blind));
        }
        // :4003, :4021 temp_blind.hover / stop_hover: the sprite's own class; the upvalue `discovered` goes to its dyn
        tsetb(NODE(temp_blind)->object.dyn, "discovered", truthy(discovered));
        if (!UIDEF_blind_sprite_class.ready) class_init(&UIDEF_blind_sprite_class);
        NODE(temp_blind)->object.cls = &UIDEF_blind_sprite_class;

        Table *row = ttabi(blind_matrix, (int)ceil((k - 1) / 5.0 + 0.001));
        if (!row) ERROR("UI_definitions.lua:4023 attempt to index a nil row of blind_matrix (more than 30 blinds)");
        // the conditional entries are nil holes: the object stays at index 2
        Table *nodes = tnew();
        if (k == 6 || k == 16 || k == 26) tseti(nodes, 1, T(TAB("n", N(UIT_B), "config", T(TAB("h", N(0.2), "w", N(0.5))))));
        tseti(nodes, 2, T(TAB("n", N(UIT_O), "config", T(TAB("object", O(temp_blind), "focus_with_object", B(true))))));
        if (k == 5 || k == 15 || k == 25) tseti(nodes, 3, T(TAB("n", N(UIT_B), "config", T(TAB("h", N(0.2), "w", N(0.5))))));
        tseti(row, 1 + ((k - 1) % 5), T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(nodes))));
    }

    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_alert_sprites, .ctx = blinds_to_be_alerted}), NULL, false);

    Table *furthest_ante = ttab(ttab(ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")), "high_scores"), "furthest_ante");
    Table *ante_amounts = tnew();
    for (int i = 1; i <= lmin(16, lmax(16, tnum(furthest_ante, "amt"))); i++) {
        num spacing = 1 - lmin(20, lmax(15, tnum(furthest_ante, "amt"))) * 0.06;
        if (spacing > 0 && i > 1) {
            tseti(ante_amounts, tlen(ante_amounts) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("minh", N(spacing))), "nodes", T(tnew()))));
        }
        Sprite *blind_chip = NEW(Sprite);
        Sprite_init(blind_chip, 0, 0, 0.2f, 0.2f, ASSET_ATLAS_get(fmt("ui_%d", tbool(G->SETTINGS, "colourblind_option") ? 2 : 1)), TAB("x", N(0), "y", N(0)));
        blind_chip->moveable.node.states.drag.can = false;
        tseti(ante_amounts, tlen(ante_amounts) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.03))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.7))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", N(i), "scale", N(0.4), "colour", P(G->C.FILTER), "shadow", B(true)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "minw", N(2.8))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(blind_chip))))),
                T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.03), "minh", N(0.01))), "nodes", T(tnew()))),
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(number_format(N(get_blind_amount(i)))), "scale", N(0.4), "colour", P(i <= tnum(furthest_ante, "amt") ? G->C.RED : G->C.JOKER_GREY), "shadow", B(true)))))
            ))))
        )))));
    }

    // :4064 `local extras = nil` is not used
    Table *t = create_UIBox_generic_options(TAB("back_func", S(exit ? exit : "your_collection"), "contents", T(ARR(
        T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.BLACK), "padding", N(0.1), "emboss", N(0.05))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.1), "colour", P(G->C.L_BLACK), "padding", N(0.1), "force_focus", B(true), "focus_args", T(TAB("nav", S("tall"))))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "minw", N(0.7))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_ante_cap"), NULL), "scale", N(0.4), "colour", P(lighten(G->C.FILTER, 0.2f, NULL)), "shadow", B(true)))))
                    )))),
                    T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cr"), "minw", N(2.8))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("k_base_cap"), NULL), "scale", N(0.4), "colour", P(lighten(G->C.RED, 0.2f, NULL)), "shadow", B(true)))))
                    ))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ante_amounts)))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 1)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 2)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 3)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 4)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 5)))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ttabi(blind_matrix, 6))))
                ))))
            ))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:4093
Table *create_UIBox_blind_popup(Table *blind, bool discovered, Table *vars) {
    Table *blind_text = tnew();

    Val _dollars = tget(blind, "dollars");
    Val loc_target = localize(T(TAB("type", S("raw_descriptions"), "key", tget(blind, "key"), "set", S("Blind"), "vars", T(vars ? vars : ttab(blind, "vars")))), NULL);
    Val loc_name = localize(T(TAB("type", S("name_text"), "key", tget(blind, "key"), "set", S("Blind"))), NULL);

    if (discovered) {
        Table *ability_text = tnew();
        if (truthy(loc_target)) {
            for (int k = 1; truthy(tgeti(loc_target.tab, k)); k++) {
                Val v = tgeti(loc_target.tab, k);
                tseti(ability_text, tlen(ability_text) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s%s", (k == 1 && streq(tstr(blind, "name"), "The Wheel")) ? "1" : "", vstr(v))), "scale", N(0.35), "shadow", B(true), "colour", P(G->C.WHITE)))))
                )))));
            }
        }
        Val stake = tget(G->GAME, "stake");
        Sprite *stake_sprite = get_stake_sprite(truthy(stake) ? (int)vnum(stake) : 1, 0.4f);
        DynaText *reward = DynaText_new(TAB("string", T(ARR(truthy(_dollars) ? S(str_rep(localize_s("$", NULL), (int)vnum(_dollars))) : S("-"))), "colours", T(ARR(P(G->C.MONEY))), "rotate", B(true), "bump", B(true), "silent", B(true), "scale", N(0.45)));
        Table *nodes = ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(2.4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("ph_blind_score_at_least"), NULL), "scale", N(0.35), "colour", P(G->C.UI.TEXT_DARK)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(stake_sprite))))),
                // blind.mult..localize('k_x_base'): the number as tostring writes it
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(fmt("%s%s", vstr(tget(blind, "mult")), vstr(localize(S("k_x_base"), NULL)))), "scale", N(0.4), "colour", P(G->C.RED)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("ph_blind_reward"), NULL), "scale", N(0.35), "colour", P(G->C.UI.TEXT_DARK))))),
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(reward)))))
            ))))
        );
        if (truthy(tgeti(ability_text, 1))) {
            tseti(nodes, 4, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.08), "colour", P(mix_colours(UI_colour(blind, "boss_colour"), G->C.GREY, 0.4f, NULL)), "r", N(0.1), "emboss", N(0.05), "minw", N(2.5), "minh", N(0.9))), "nodes", T(ability_text))));
        }
        tseti(blind_text, tlen(blind_text) + 1,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "emboss", N(0.05), "r", N(0.1), "minw", N(2.5), "padding", N(0.07), "colour", P(G->C.WHITE))), "nodes", T(nodes))));
    } else {
        tseti(blind_text, tlen(blind_text) + 1,
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "emboss", N(0.05), "r", N(0.1), "minw", N(2.5), "padding", N(0.1), "colour", P(G->C.WHITE))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("ph_defeat_this_blind_1"), NULL), "scale", N(0.4), "colour", P(G->C.UI.TEXT_DARK)))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_T), "config", T(TAB("text", localize(S("ph_defeat_this_blind_2"), NULL), "scale", N(0.4), "colour", P(G->C.UI.TEXT_DARK)))))
                ))))
            )))));
    }
    float *root_colour = lighten(G->C.JOKER_GREY, 0.5f, NULL);
    float *boss_colour = UI_colour(blind, "boss_colour");
    // string is a lone value (a name, or a table of lines): DynaText_init wraps it
    DynaText *name = DynaText_new(TAB("string", discovered ? loc_name : localize(S("k_not_discovered"), NULL), "colours", T(ARR(P(G->C.UI.TEXT_LIGHT))), "shadow", B(true), "rotate", B(!discovered), "spacing", N(discovered ? 2 : 0), "bump", B(true), "scale", N(0.4)));
    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "padding", N(0.05), "colour", P(root_colour), "r", N(0.1), "emboss", N(0.05))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "emboss", N(0.05), "r", N(0.1), "minw", N(2.5), "padding", N(0.1), "colour", P(!discovered ? G->C.JOKER_GREY : boss_colour ? boss_colour : G->C.GREY))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(name)))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"))), "nodes", T(blind_text)))
    )));
}

// UI_definitions.lua:4159-4194 the eight events of the unlock's reveal
struct ev_card_unlock_ctx {
    int step; // 1..8, in the Lua's order
    Card *card, *locked_card;
    int ID;   // card.ID
};
// G.OVERLAY_MENU and G.OVERLAY_MENU.joker_unlock_table == card.ID
static bool card_unlock_showing(int ID) {
    if (!G->OVERLAY_MENU) return false;
    if (G->OVERLAY_MENU == OVERLAY_MENU_TRUE) ERROR("UI_definitions.lua:4162 attempt to index G.OVERLAY_MENU (a boolean value)");
    return veq(tget(G->OVERLAY_MENU->moveable.node.object.dyn, "joker_unlock_table"), N(ID));
}
static bool ev_card_unlock(void *ctx) {
    struct ev_card_unlock_ctx *c = ctx;
    Card *card = c->card, *locked_card = c->locked_card;
    if (c->step == 1) {
        // :4160 no nil check in the Lua
        if (!G->OVERLAY_MENU || G->OVERLAY_MENU == OVERLAY_MENU_TRUE) ERROR("UI_definitions.lua:4160 attempt to index G.OVERLAY_MENU (not a UIBox)");
        tsetn(G->OVERLAY_MENU->moveable.node.object.dyn, "joker_unlock_table", c->ID);
        return true;
    }
    if (!card_unlock_showing(c->ID)) return true;
    switch (c->step) {
    case 2: Card_juice_up(locked_card, 0.3f, N(0.2)); play_sound("cancel", 0.8f, -1); break;   // :4162
    case 3: Card_juice_up(locked_card, 0.45f, N(0.3)); play_sound("cancel", 0.92f, -1); break; // :4164
    case 4: Card_juice_up(locked_card, 0.6f, N(0.4)); play_sound("cancel", 1.03f, -1); break;  // :4166
    case 5: Card_start_dissolve(locked_card, ARR(P(G->C.BLACK)), false, -1, false); break;      // :4175
    case 6:                                                                                     // :4186
        Card_start_materialize(card, ARR(P(G->C.BLUE)), true, -1);
        play_sound("crumple1", 0.8f, 1);
        break;
    case 7: card->no_ui = false; play_sound("timpani", 0.8f, 1.8f); break;                      // :4192
    case 8: play_sound("timpani", 1, 1.8f); break;                                              // :4194
    }
    return true;
}

// UI_definitions.lua:4143
Table *create_UIBox_card_unlock(Table *card_center) {
    CardArea *your_collection = NEW(CardArea);
    CardArea_init(your_collection, G->ROOM->T->x + G->ROOM->T->w / 2, G->ROOM->T->h,
                  1 * G->CARD_W,
                  1 * G->CARD_H,
                  TAB("card_limit", N(2), "type", S("consumeable"), "highlight_limit", N(0)));
    G->your_collection = O(your_collection);
    Transform *aT = your_collection->moveable.node.T;

    Card *card = NEW(Card);
    Card_init(card, aT->x + aT->w / 2 - G->CARD_W / 2, aT->y, G->CARD_W, G->CARD_H, NULL, card_center, TAB("bypass_discovery_center", B(true), "bypass_discovery_ui", B(true)));
    Card *locked_card = NEW(Card);
    Card_init(locked_card, aT->x + aT->w / 2 - G->CARD_W / 2, aT->y, G->CARD_W, G->CARD_H, NULL, streq(tstr(card_center, "set"), "Voucher") ? G->v_locked : G->j_locked, NULL);
    Card_remove_UI(locked_card);
    NODE(locked_card)->ID = NODE(card)->ID;
    NODE(card)->states.click.can = false;
    NODE(locked_card)->states.click.can = false;
    NODE(card)->states.visible = false;
    card->no_ui = true;

    // note: the closures read card.ID when they run; it never changes after this point, so it is captured by value
    // (the cards may be gone by then: they are only touched while the overlay still shows this unlock).
    static const num delays[8] = {0, 0.6, 1.15, 1.8, 2.3, 2.7, 2.78, 2.95};
    for (int step = 1; step <= 8; step++) {
        struct ev_card_unlock_ctx *c = NEWCTX(struct ev_card_unlock_ctx);
        c->step = step, c->card = card, c->locked_card = locked_card, c->ID = NODE(card)->ID;
        E_add(G->E_MANAGER, Event_new((EventCfg){.timer = "REAL", .not_blockable = true, .not_blocking = true,
                                                 .trigger = step == 1 ? NULL : "after", .delay = delays[step - 1],
                                                 .func = ev_card_unlock, .ctx = c}), NULL, false);
    }

    CardArea_emplace(your_collection, card, NULL, false);
    CardArea_emplace(your_collection, locked_card, NULL, false);

    Val back_label = localize(S("b_continue"), NULL);
    DynaText *kind = DynaText_new(TAB("string", T(ARR(streq(tstr(card_center, "set"), "Voucher") ? localize(S("k_voucher"), NULL) : localize(S("k_joker"), NULL))), "colours", T(ARR(P(G->C.BLUE))), "shadow", B(true), "rotate", B(true), "bump", B(true), "pop_in", N(0.3), "pop_in_rate", N(2), "scale", N(1.2)));
    DynaText *unlocked = DynaText_new(TAB("string", T(ARR(localize(S("k_unlocked_ex"), NULL))), "colours", T(ARR(P(G->C.RED))), "shadow", B(true), "rotate", B(true), "bump", B(true), "pop_in", N(0.6), "pop_in_rate", N(2), "scale", N(0.8)));
    Table *desc = desc_from_rows(ttab(Card_generate_UIBox_unlock_table(card, true), "main"), false, NIL);

    Table *t = create_UIBox_generic_options(TAB("padding", N(0), "back_label", back_label, "no_pip", B(true), "snap_back", B(true), "back_func", S("continue_unlock"), "minw", N(4.5), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(kind)))))
                    )))),
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_O), "config", T(TAB("object", O(unlocked)))))
                    ))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0), "draw_layer", N(1))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_O), "config", T(TAB("object", O(your_collection)))))
                )))),
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.2))), "nodes", T(ARR(
                    T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.05), "emboss", N(0.05), "colour", P(G->C.WHITE), "r", N(0.1))), "nodes", T(ARR(
                        T(desc)
                    ))))
                ))))
            ))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:4224
Table *create_UIBox_deck_unlock(Table *deck_center) {
    Back *viewed_back = NEW(Back);
    Back_init(viewed_back, deck_center);
    tseto(G->GAME, "viewed_back", viewed_back);
    CardArea *area = NEW(CardArea);
    CardArea_init(area, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h,
                  1.2f * G->CARD_W,
                  1.2f * G->CARD_H,
                  TAB("card_limit", N(52), "type", S("deck"), "highlight_limit", N(0)));

    for (int i = 1; i <= 52; i++) {
        Card *card = NEW(Card);
        Card_init(card, G->ROOM->T->x + 0.2f * G->ROOM->T->w / 2, G->ROOM->T->h, G->CARD_W * 1.2f, G->CARD_H * 1.2f,
                  pseudorandom_element(G->P_CARDS, NIL, NULL).tab, ttab(G->P_CENTERS, "c_base"),
                  TAB("bypass_back", tget(deck_center, "pos"), "playing_card", N(i), "viewed_back", B(true)));
        CardArea_emplace(area, card, NULL, false);
        tset(NODE(card)->object.dyn, tstr(deck_center, "key"), B(true)); // card[deck_center.key] = true
        card->sprite_facing = "back";
        card->facing = "back";
    }
    Table *deck_criteria = tnew();
    Table *unlock_condition = ttab(deck_center, "unlock_condition");
    if (streq(tstr(unlock_condition, "type"), "win_deck")) {
        Val other_name = localize(T(TAB("type", S("name_text"), "set", S("Back"), "key", tget(unlock_condition, "deck"))), NULL);
        localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_win"), "set", S("Other"), "nodes", T(deck_criteria), "vars", T(ARR(other_name)), "default_col", P(G->C.WHITE), "shadow", B(true))), NULL);
    } else if (streq(tstr(unlock_condition, "type"), "win_stake")) {
        Val other_name = localize(T(TAB("type", S("name_text"), "set", S("Stake"), "key", tget(ttabi(ttab(G->P_CENTER_POOLS, "Stake"), (int)tnum(unlock_condition, "stake")), "key"))), NULL);
        Table *vars = ARR(other_name);
        tsett(vars, "colours", ARR(P(get_stake_col((int)tnum(unlock_condition, "stake")))));
        localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_stake"), "set", S("Other"), "nodes", T(deck_criteria), "vars", T(vars), "default_col", P(G->C.WHITE), "shadow", B(true))), NULL);
    } else if (streq(tstr(unlock_condition, "type"), "discover_amount")) {
        localize(T(TAB("type", S("descriptions"), "key", S("deck_locked_discover"), "set", S("Other"), "nodes", T(deck_criteria), "vars", T(ARR(tget(unlock_condition, "amount"))), "default_col", P(G->C.WHITE), "shadow", B(true))), NULL);
    }

    Table *deck_criteria_cols = tnew();
    for (int k = 1; truthy(tgeti(deck_criteria, k)); k++) {
        Val v = tgeti(deck_criteria, k);
        if (k > 1) tseti(deck_criteria_cols, tlen(deck_criteria_cols) + 1, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0), "minw", N(0.1))), "nodes", T(tnew()))));
        tseti(deck_criteria_cols, tlen(deck_criteria_cols) + 1, T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", v)));
    }

    Val back_label = localize(S("b_continue"), NULL);
    DynaText *title = DynaText_new(TAB("string", T(ARR(T(TAB("string", localize(T(TAB("type", S("name_text"), "set", S("Back"), "key", tget(deck_center, "key"))), NULL), "suffix", S(fmt(" %s", vstr(localize(S("k_unlocked_ex"), NULL)))), "outer_colour", P(G->C.UI.TEXT_LIGHT))))), "colours", T(ARR(P(G->C.BLUE))), "shadow", B(true), "rotate", B(true), "float", B(true), "scale", N(0.7), "pop_in", N(0.1)));
    Table *back_nodes = ttab(Back_generate_UI(viewed_back, deck_center, -1, -1, NULL), "nodes");

    Table *t = create_UIBox_generic_options(TAB("back_label", back_label, "no_pip", B(true), "snap_back", B(true), "back_func", S("continue_unlock"), "minw", N(7), "contents", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(title)))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(deck_criteria_cols))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.2), "colour", P(G->C.BLACK), "r", N(0.2))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("object", O(area)))))
            )))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "r", N(0.2), "colour", P(G->C.WHITE), "emboss", N(0.05), "padding", N(0.2), "minw", N(4))), "nodes", T(ARR(
                T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(back_nodes)))
            ))))
        ))))
    ))));
    return t;
}

// UI_definitions.lua:4273
Table *UIDEF_multiline_credit_text(Table *_lines) {
    Table *t = tnew();
    for (int k = 1; truthy(tgeti(_lines, k)); k++) {
        Val v = tgeti(_lines, k);
        tseti(t, tlen(t) + 1, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", v, "scale", N(0.28), "colour", P(G->C.WHITE)))))
        )))));
    }

    return t;
}

// UI_definitions.lua:4284
Table *UIDEF_viewed_collab_option(void) {
    // note: the texts of this function are string literals of the Lua: LITERALS.collab_option of data/game.dat (data.h):
    // 1 the collaboration shown by default, 2-7 the lines about the rights, 8 the heading over the artist
    Table *x = lits("collab_option");
    G->viewed_collab = G->viewed_collab ? G->viewed_collab : lit_at(x, 1);

    Table *curr_collab = ttab(G->collab_credits, G->viewed_collab) ? ttab(G->collab_credits, G->viewed_collab) : ttab(G->collab_credits, lit_at(x, 1));
    Sprite *collab_sprite = NEW(Sprite);
    Sprite_init(collab_sprite, 0, 0, 0.8f * G->CARD_W, 0.8f * G->CARD_H, ASSET_ATLAS_get(fmt("%s_1", vstr(tget(curr_collab, "art")))), TAB("x", N(2), "y", N(0)));

    // the conditional entries (`cond and {...} or nil`) are all the last of their list: no hole to keep
    Table *text_nodes = ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "padding", N(0))), "nodes", T(UIDEF_multiline_credit_text(ARR(
            S(lit_at(x, 2)), S(lit_at(x, 3)), S(lit_at(x, 4)), S(lit_at(x, 5)), S(lit_at(x, 6)), S(lit_at(x, 7))
        )))))
    );
    if (truthy(tget(curr_collab, "ml_text"))) {
        tseti(text_nodes, 2, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cl"), "padding", N(0))), "nodes", T(UIDEF_multiline_credit_text(ttab(curr_collab, "ml_text"))))));
    }

    Table *art_nodes = ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "colour", P(G->C.WHITE), "r", N(0.3))), "nodes", T(ARR(
                T(TAB("n", N(UIT_O), "config", T(TAB("colour", P(G->C.BLUE), "object", O(collab_sprite), "hover", B(true), "can_collide", B(false)))))
            ))))
        ))))
    );
    if (truthy(tget(curr_collab, "artist"))) {
        Table *artist_nodes = ARR(
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", S(lit_at(x, 8)), "scale", N(0.3), "colour", P(G->C.WHITE)))))
            )))),
            T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(curr_collab, "artist"), "scale", N(0.3), "colour", P(G->C.GOLD)))))
            ))))
        );
        if (truthy(tget(curr_collab, "artist_link"))) {
            tseti(artist_nodes, 3, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
                T(TAB("n", N(UIT_T), "config", T(TAB("text", tget(curr_collab, "artist_link"), "scale", N(0.3), "colour", P(G->C.BLUE)))))
            )))));
        }
        tseti(art_nodes, 2, T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.07), "r", N(0.1), "outline", N(0.7), "outline_colour", P(G->C.WHITE))), "nodes", T(artist_nodes))));
    }

    return TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.BLACK), "r", N(0.1))), "nodes", T(ARR(
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0.1))), "nodes", T(ARR(
            T(TAB("n", N(UIT_T), "config", T(TAB("text", S(G->viewed_collab), "scale", N(0.5), "colour", P(G->C.WHITE)))))
        )))),
        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "padding", N(0))), "nodes", T(ARR(
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minw", N(9), "minh", N(4.2))), "nodes", T(text_nodes))),
            T(TAB("n", N(UIT_C), "config", T(TAB("align", S("cm"), "padding", N(0.1), "minw", N(3), "minh", N(4.2))), "nodes", T(art_nodes)))
        ))))
    )));
}
