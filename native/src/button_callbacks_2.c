// functions/button_callbacks.lua:649-1298: the collection pages (spectral, booster, voucher), the settings cycles,
// the text entry, the window settings and the run setup watchers.
#include "all.h"

// ---------------------------------------------------------------- file-local helpers
// a or b
static inline Val vor(Val a, Val b) { return truthy(a) ? a : b; }
// the table of a value, NULL as nil (or not a table)
static inline Table *vtab(Val v) { return v.t == V_TAB ? v.tab : NULL; }
static inline Table *cfg_of(void *node) { return ((Node *)node)->config; }
static inline Table *children_of(void *node) { return ((Node *)node)->children; }
// G.your_collection as the array of card areas. note: when it holds a single CardArea (UI_definitions.lua:3785,
// :4144) the Lua's #G.your_collection is 0: NULL here, and tlen(NULL) is 0.
static inline Table *your_collection(void) { return vtab(G->your_collection); }
// G.PROFILES[G.SETTINGS.profile].MEMORY
static inline Table *profile_memory(void) { return ttab(vtab(tgetv(G->PROFILES, tget(G->SETTINGS, "profile"))), "MEMORY"); }
// text.ref_table[text.ref_value] of a text input. note: ref_table is a table or an object (the seed input binds G:
// UI_definitions.lua:6113), whose bound fields are in its dyn (ui.h's ui_ref_get, game.h's "in dyn:").
static Val text_ref_get(Table *text) { return ui_ref_get(tget(text, "ref_table"), tget(text, "ref_value")); }
static void text_ref_set(Table *text, Val v) {
    Val rt = tget(text, "ref_table");
    if (rt.t == V_OBJ) tsetv(((Object *)rt.obj)->dyn, tget(text, "ref_value"), v);
    else tsetv(vtab(rt), tget(text, "ref_value"), v);
}
// copy_table of a colour kept in a table: 4 new floats (never freed, as HEX and friends); nil stays nil
static Val colour_copy(Val colour) {
    if (colour.t != V_PTR) return colour;
    float *c = malloc(sizeof(Colour));
    memcpy(c, colour.p, sizeof(Colour));
    return P(c);
}
// child.config.id:sub(1, 7) == 'letter_' and child.config.text ~= ''
static bool is_real_letter(Node *child) {
    return streq(str_sub(vstr(tget(child->config, "id")), 1, 7), "letter_") && !veq(tget(child->config, "text"), S(""));
}
// G.CONTROLLER.text_input_hook, which the Lua indexes without a test
static UIElement *text_hook(void) {
    UIElement *hook = G->CONTROLLER->text_input_hook;
    if (!hook) ERROR("attempt to index field 'text_input_hook' (a nil value)");
    return hook;
}

// ---------------------------------------------------------------- collection pages
// button_callbacks.lua:649
void FUNCS_your_collection_spectral_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = CardArea_n(area); i >= 1; i--) {
            Card *c = CardArea_remove_card(area, CardArea_card(area, i), false);
            CALL(c, remove);
        }
    }

    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = 1; i <= 3 + j; i++) {
            Table *center = vtab(tgetv(ttab(G->P_CENTER_POOLS, "Spectral"),
                                       N(i + (j - 1) * (4) + (9 * (tnum(ttab(args, "cycle_config"), "current_option") - 1)))));
            if (!center) break;
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H,
                      ttab(G->P_CARDS, "empty"), center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }
    INIT_COLLECTION_CARD_ALERTS();
}

// button_callbacks.lua:675
void FUNCS_your_collection_booster_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = CardArea_n(area); i >= 1; i--) {
            Card *c = CardArea_remove_card(area, CardArea_card(area, i), false);
            CALL(c, remove);
        }
    }

    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = 1; i <= 4; i++) {
            Table *center = vtab(tgetv(ttab(G->P_CENTER_POOLS, "Booster"),
                                       N(i + (j - 1) * 4 + (8 * (tnum(ttab(args, "cycle_config"), "current_option") - 1)))));
            if (!center) break;
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W * 1.27f,
                      G->CARD_H * 1.27f, NULL, center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }
    INIT_COLLECTION_CARD_ALERTS();
}

// button_callbacks.lua:701
void FUNCS_your_collection_voucher_page(Table *args) {
    if (!args || !truthy(tget(args, "cycle_config"))) return;
    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = CardArea_n(area); i >= 1; i--) {
            Card *c = CardArea_remove_card(area, CardArea_card(area, i), false);
            CALL(c, remove);
        }
    }
    for (int i = 1; i <= 4; i++) {
        for (int j = 1; j <= tlen(your_collection()); j++) {
            CardArea *area = tobji(your_collection(), j);
            Table *center = vtab(tgetv(ttab(G->P_CENTER_POOLS, "Voucher"),
                                       N(i + (j - 1) * 4 + (8 * (tnum(ttab(args, "cycle_config"), "current_option") - 1)))));
            if (!center) break;
            Card *card = NEW(Card);
            Card_init(card, area->moveable.node.T->x + area->moveable.node.T->w / 2, area->moveable.node.T->y, G->CARD_W, G->CARD_H,
                      ttab(G->P_CARDS, "empty"), center, NULL);
            Card_start_materialize(card, NULL, i > 1 || j > 1, -1);
            CardArea_emplace(area, card, NULL, false);
        }
    }
    INIT_COLLECTION_CARD_ALERTS();
}

// ---------------------------------------------------------------- cycle callbacks
// button_callbacks.lua:726
void FUNCS_change_selected_back(Table *args) {
    Back_change_to(tobj(G->GAME, "selected_back"), vtab(tgetv(ttab(G->P_CENTER_POOLS, "Back"), tget(args, "to_key"))));
}

// button_callbacks.lua:734
void FUNCS_change_viewed_back(Table *args) {
    if (!G->viewed_stake) G->viewed_stake = 1;
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    Back_change_to(viewed_back, vtab(tgetv(ttab(G->P_CENTER_POOLS, "Back"), tget(args, "to_key"))));
    if (G->sticker_card) G->sticker_card->sticker = get_deck_win_sticker(ttab(viewed_back->effect, "center"));
    // note: get_deck_win_stake with a key never returns nil (0: none), so the Lua's "or 0" has nothing to do
    num max_stake = get_deck_win_stake(tstr(ttab(viewed_back->effect, "center"), "key"), NULL);
    G->viewed_stake = (int)lmin(G->viewed_stake, max_stake + 1);
    tset(profile_memory(), "deck", tget(args, "to_val"));
}

// button_callbacks.lua:743
void FUNCS_change_stake(Table *args) {
    G->viewed_stake = (int)tnum(args, "to_key");
    tset(profile_memory(), "stake", tget(args, "to_key"));
}

// button_callbacks.lua:752
void FUNCS_change_vsync(Table *args) {
    Val vsync = tget(ttab(G->SETTINGS, "WINDOW"), "vsync"), to_key = tget(args, "to_key");
    tset(ttab(G->SETTINGS, "QUEUED_CHANGE"), "vsync",
         (veq(vsync, N(0)) && veq(to_key, N(1))) ? N(1) : (veq(vsync, N(1)) && veq(to_key, N(2))) ? N(0) : NIL);
}

// button_callbacks.lua:763
void FUNCS_change_screen_resolution(Table *args) {
    Table *WINDOW = ttab(G->SETTINGS, "WINDOW");
    Val curr_disp = tget(WINDOW, "selected_display");
    Table *to_resolution =
        vtab(tgetv(ttab(ttab(vtab(tgetv(ttab(WINDOW, "DISPLAYS"), curr_disp)), "screen_resolutions"), "values"), tget(args, "to_key")));
    if (!to_resolution) ERROR("attempt to index local 'to_resolution' (a nil value)");
    tsett(ttab(G->SETTINGS, "QUEUED_CHANGE"), "screenres", TAB("w", tget(to_resolution, "w"), "h", tget(to_resolution, "h")));
}

// button_callbacks.lua:776
void FUNCS_change_screenmode(Table *args) {
    if (!truthy(tget(G->ARGS, "screenmode_vals")))
        tsett(G->ARGS, "screenmode_vals", ARR(S("Windowed"), S("Fullscreen"), S("Borderless")));
    tset(ttab(G->SETTINGS, "QUEUED_CHANGE"), "screenmode", tgetv(ttab(G->ARGS, "screenmode_vals"), tget(args, "to_key")));
    FUNCS_change_window_cycle_UI();
}

// button_callbacks.lua:787
void FUNCS_change_display(Table *args) {
    tset(ttab(G->SETTINGS, "QUEUED_CHANGE"), "selected_display", tget(args, "to_key"));
    FUNCS_change_window_cycle_UI();
}

// button_callbacks.lua:793
void FUNCS_change_window_cycle_UI(void) {
    if (G->OVERLAY_MENU) {
        UIElement *swap_node = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("resolution_cycle"), NULL);
        if (swap_node) {
            Table *WINDOW = ttab(G->SETTINGS, "WINDOW"), *QUEUED_CHANGE = ttab(G->SETTINGS, "QUEUED_CHANGE");
            Val focused_display = vor(tget(QUEUED_CHANGE, "selected_display"), tget(WINDOW, "selected_display"));
            Val focused_screenmode = vor(tget(QUEUED_CHANGE, "screenmode"), tget(WINDOW, "screenmode"));

            // Refresh the display information
            int res_option = GET_DISPLAYINFO(vstr(focused_screenmode), (int)vnum(focused_display));

            // Remove the old cycle, replace it with a new updated one reflecting any changes
            CALL(tobji(children_of(swap_node), 1), remove);
            tseti(children_of(swap_node), 1, NIL);
            // note: GET_DISPLAYINFO returns an int, never nil: "res_option or 1" is res_option
            UIBox_add_child(swap_node->UIBox,
                            create_option_cycle(TAB("w", N(4), "scale", N(0.8), "options",
                                                    tget(ttab(vtab(tgetv(ttab(WINDOW, "DISPLAYS"), focused_display)), "screen_resolutions"),
                                                         "strings"),
                                                    "opt_callback", S("change_screen_resolution"), "current_option", N(res_option))),
                            swap_node);
        }
    }
}

// button_callbacks.lua:817
void FUNCS_change_gamespeed(Table *args) { tset(G->SETTINGS, "GAMESPEED", tget(args, "to_val")); }

// button_callbacks.lua:826
void FUNCS_change_play_discard_position(Table *args) {
    tset(G->SETTINGS, "play_button_pos", tget(args, "to_key"));
    if (G->buttons) {
        CALL(G->buttons, remove);
        G->buttons = UIBox_new(TAB("definition", T(create_UIBox_buttons()), "config",
                                   T(TAB("align", S("bm"), "offset", T(TAB("x", N(0), "y", N(0.3))), "major", O(G->hand), "bond", S("Weak")))));
    }
}

// button_callbacks.lua:842
void FUNCS_change_shadows(Table *args) {
    tsets(ttab(G->SETTINGS, "GRAPHICS"), "shadows", veq(tget(args, "to_key"), N(1)) ? "On" : "Off");
    Game_save_settings(G);
}

// button_callbacks.lua:852
void FUNCS_change_pixel_smoothing(Table *args) {
    tset(ttab(G->SETTINGS, "GRAPHICS"), "texture_scaling", tget(args, "to_key"));
    Game_set_render_settings(G);
    Game_save_settings(G);
}

// button_callbacks.lua:863
void FUNCS_change_crt_bloom(Table *args) {
    tset(ttab(G->SETTINGS, "GRAPHICS"), "bloom", tget(args, "to_key"));
    Game_save_settings(G);
}

// button_callbacks.lua:868
void FUNCS_change_collab(Table *args) {
    Val curr_suit = tget(ttab(args, "cycle_config"), "curr_suit");
    tsetv(ttab(ttab(G->SETTINGS, "CUSTOM_DECK"), "Collabs"), curr_suit,
          vor(tgetv(vtab(tgetv(ttab(G->COLLABS, "options"), curr_suit)), tget(args, "to_key")), S("default")));
    // note: pairs(G.I.CARD); the order does not matter. G.I.CARD also holds the Blind and the Card_Characters
    // (cardarea.h's notes), whose config has no 'card': is_a stands for that part of the Lua's test.
    for (TIter it = titer(G->I.CARD); tnext(&it);) {
        if (!is_a(it.val.obj, &Card_class)) continue;
        Card *v = it.val.obj;
        if (cfg_of(v) && truthy(tget(cfg_of(v), "card")) && Card_child(v, "front") && !streq(tstr(v->ability, "effect"), "Stone Card")) {
            Card_set_sprites(v, NULL, ttab(cfg_of(v), "card"));
        }
    }
    Game_save_settings(G);
}

// ---------------------------------------------------------------- text entry
// button_callbacks.lua:887
void FUNCS_key_button(UIElement *e) {
    Table *args = ttab(cfg_of(e), "ref_table");
    if (truthy(tget(args, "key"))) Controller_key_press_update(G->CONTROLLER, tstr(args, "key"), 0);
}

// button_callbacks.lua:899
void FUNCS_text_input(UIElement *e) {
    Table *args = ttab(cfg_of(e), "ref_table");
    Node *pp = e->moveable.node.parent->parent; // e.parent.parent
    if (G->CONTROLLER->text_input_hook == e) {
        tset(pp->config, "colour", tget(args, "hooked_colour"));
        tsets(args, "current_prompt_text", "");
        tset(args, "current_position_text", tget(args, "position_text"));
    } else {
        tset(pp->config, "colour", tget(args, "colour"));
        tset(args, "current_prompt_text", veq(text_ref_get(ttab(args, "text")), S("")) ? vor(tget(args, "prompt_text"), S("")) : S(""));
        tsets(args, "current_position_text", "");
    }

    Node *OSkeyboard_e = pp->parent;
    if (G->CONTROLLER->text_input_hook == e && G->CONTROLLER->HID.controller) {
        if (!tobj(OSkeyboard_e->children, "controller_keyboard")) {
            UIBox *keyboard = UIBox_new(TAB(
                "definition", T(create_keyboard_input(TAB("backspace_key", B(true), "return_key", B(true), "space_key", B(false)))),
                "config",
                T(TAB("align", S("cm"), "offset",
                      T(TAB("x", N(0), "y", vor(tget(ttab(cfg_of(G->CONTROLLER->text_input_hook), "ref_table"), "keyboard_offset"), N(-4)))),
                      "major", O(e->UIBox), "parent", O(OSkeyboard_e)))));
            tseto(OSkeyboard_e->children, "controller_keyboard", keyboard);
            G->CONTROLLER->screen_keyboard = keyboard;
            Controller_mod_cursor_context_layer(G->CONTROLLER, 1);
        }
    } else if (tobj(OSkeyboard_e->children, "controller_keyboard")) {
        CALL(tobj(OSkeyboard_e->children, "controller_keyboard"), remove);
        tset(OSkeyboard_e->children, "controller_keyboard", NIL);
        G->CONTROLLER->screen_keyboard = NULL;
        Controller_mod_cursor_context_layer(G->CONTROLLER, -1);
    }
}

// button_callbacks.lua:932
void FUNCS_paste_seed(UIElement *e) {
    UIElement *text_input = UIBox_get_UIE_by_ID(e->UIBox, S("text_input"), NULL);
    if (!text_input) ERROR("attempt to index a nil value (get_UIE_by_ID('text_input'))");
    G->CONTROLLER->text_input_hook = tobji(children_of(tobji(children_of(text_input), 1)), 1);
    for (int i = 1; i <= 8; i++) {
        FUNCS_text_input_key(TAB("key", S("right")));
    }
    for (int i = 1; i <= 8; i++) {
        FUNCS_text_input_key(TAB("key", S("backspace")));
    }
    const char *clipboard = (G->F_LOCAL_CLIPBOARD && G->CLIPBOARD) ? G->CLIPBOARD : love_system_getClipboardText();
    if (!clipboard) clipboard = "";
    for (int i = 1; i <= str_len(clipboard); i++) {
        const char *c = str_sub(clipboard, i, i);
        FUNCS_text_input_key(TAB("key", S(c)));
    }
    FUNCS_text_input_key(TAB("key", S("return")));
}

// button_callbacks.lua:953
void FUNCS_select_text_input(UIElement *e) {
    G->CONTROLLER->text_input_hook = tobji(children_of(tobji(children_of(e), 1)), 1);

    // Start by setting the cursor position to the correct location
    TRANSPOSE_TEXT_INPUT(0);
    UIBox_recalculate(e->UIBox);
}

// button_callbacks.lua:980, the keymap table: NULL as nil
static const char *keymap(const char *key) {
    if (streq(key, "space")) return " ";
    if (streq(key, "backspace")) return "BACKSPACE";
    if (streq(key, "delete")) return "DELETE";
    if (streq(key, "return")) return "RETURN";
    if (streq(key, "right")) return "RIGHT";
    if (streq(key, "left")) return "LEFT";
    return NULL;
}

// button_callbacks.lua:994, string.find(lower_ext, key): a PATTERN find. Returns the start (0: no match) and the
// end in *e.
// note: core.h has no pattern matcher, so the cases are written out. One character: '.' matches at 1; '^' matches
// empty at 1 and '$' empty at the end (the key then becomes ''); '%', '(' and ')' are malformed patterns, an error
// in the Lua ('%' is also the default of a nil key); '[' and ']' never get here (:969); any other character is
// itself. Longer keys are key names ('backspace', 'lshift', 'kp1'...), which all start with a letter that
// lower_ext does not have: a plain find gives the same "no match".
static int lower_ext_find(const char *lower_ext, const char *key, int *e) {
    int len = str_len(key);
    if (len == 0) return *e = 0, 1;
    if (len == 1) {
        switch (key[0]) {
        case '%': ERROR("malformed pattern (ends with '%%')");
        case '(': ERROR("unfinished capture");
        case ')': ERROR("invalid pattern capture");
        case '^': return *e = 0, 1;
        case '$': return *e = str_len(lower_ext), str_len(lower_ext) + 1;
        case '.': return *e = 1, 1;
        }
    }
    int s = str_find(lower_ext, key, 1);
    *e = s + len - 1;
    return s;
}

// button_callbacks.lua:966
void FUNCS_text_input_key(Table *args) {
    if (!args) args = tnew();

    if (streq(tstr(args, "key"), "[") || streq(tstr(args, "key"), "]")) return;
    if (streq(tstr(args, "key"), "0")) tsets(args, "key", "o");

    // shortcut to hook config
    Table *hook_config = ttab(cfg_of(text_hook()), "ref_table");
    if (!truthy(tget(hook_config, "orig_colour"))) tset(hook_config, "orig_colour", colour_copy(tget(hook_config, "colour")));

    if (!truthy(tget(args, "key"))) tsets(args, "key", "%");
    // capitalize if caps lock or hook requires
    tset(args, "caps", vor(tget(args, "caps"), G->CONTROLLER->capslock ? B(true) : tget(hook_config, "all_caps")));

    // Some special keys need to be mapped accordingly before passing through the corpus: keymap() above
    UIElement *hook = G->CONTROLLER->text_input_hook;
    const char *corpus = fmt("%s%s", "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
                             tbool(ttab(cfg_of(hook), "ref_table"), "extended_corpus") ? " 0!$&()<>?:{}+-=,.[]_" : "");

    if (tbool(ttab(cfg_of(hook), "ref_table"), "extended_corpus")) {
        const char *lower_ext = "1234567890-=;',./";
        const char *upper_ext = "!@#$%^&*()_+:\"<>?";
        int s, e;
        if ((s = lower_ext_find(lower_ext, tstr(args, "key"), &e)) && tbool(args, "caps")) {
            tsets(args, "key", str_sub(str_sub(upper_ext, s, e), 0, 1));
        }
    }
    Table *text = ttab(hook_config, "text");

    // set key to mapped key or upper if caps is true
    const char *key = tstr(args, "key");
    key = keymap(key) ? keymap(key) : (tbool(args, "caps") ? str_upper(key) : key);
    tsets(args, "key", key);

    // Start by setting the cursor position to the correct location
    TRANSPOSE_TEXT_INPUT(0);

    if (str_len(vstr(text_ref_get(text))) > 0 && streq(key, "BACKSPACE")) { // If not at start, remove preceding letter
        MODIFY_TEXT_INPUT(TAB("letter", S(""), "text_table", T(text), "pos", tget(text, "current_position"), "delete", B(true)));
        TRANSPOSE_TEXT_INPUT(-1);
    } else if (str_len(vstr(text_ref_get(text))) > 0 && streq(key, "DELETE")) { // if not at end, remove following letter
        MODIFY_TEXT_INPUT(TAB("letter", S(""), "text_table", T(text), "pos", N(tnum(text, "current_position") + 1), "delete", B(true)));
        TRANSPOSE_TEXT_INPUT(0);
    } else if (streq(key, "RETURN")) { // Release the hook
        Val callback = tget(ttab(cfg_of(hook), "ref_table"), "callback");
        if (truthy(callback)) fcall0(callback);
        tset(cfg_of(hook->moveable.node.parent->parent), "colour", tget(hook_config, "colour"));
        float *temp_colour = colour_copy(tget(hook_config, "orig_colour")).p;
        float *colour = UI_colour(hook_config, "colour");
        colour[0] = G->C.WHITE[0];
        colour[1] = G->C.WHITE[1];
        colour[2] = G->C.WHITE[2];
        ease_colour(colour, temp_colour, -1);
        G->CONTROLLER->text_input_hook = NULL;
    } else if (streq(key, "LEFT")) { // Move cursor position to the left
        TRANSPOSE_TEXT_INPUT(-1);
    } else if (streq(key, "RIGHT")) { // Move cursor position to the right
        TRANSPOSE_TEXT_INPUT(1);
    } else if (tnum(hook_config, "max_length") > str_len(vstr(text_ref_get(text))) && (str_len(key) == 1) &&
               str_find(corpus, key, 1)) { // check to make sure the key is in the valid corpus, add it to the string
        MODIFY_TEXT_INPUT(TAB("letter", S(key), "text_table", T(text), "pos", N(tnum(text, "current_position") + 1)));
        TRANSPOSE_TEXT_INPUT(1);
    }
}

// button_callbacks.lua:1048
const char *GET_TEXT_FROM_INPUT(void) {
    const char *new_text = "";
    UIElement *hook = text_hook();
    for (int i = 1; i <= tlen(children_of(hook)); i++) {
        Node *child = tobji(children_of(hook), i);
        if (child->config && is_real_letter(child)) {
            new_text = fmt("%s%s", new_text, vstr(tget(child->config, "text")));
        }
    }
    return new_text;
}

// button_callbacks.lua:1066
void MODIFY_TEXT_INPUT(Table *args) {
    if (!args) args = tnew();
    Table *letters = ttab(ttab(args, "text_table"), "letters");
    int pos = (int)tnum(args, "pos");

    if (tbool(args, "delete") && pos > 0) {
        if (pos >= tlen(letters)) {
            tseti(letters, pos, S(""));
        } else {
            tseti(letters, pos, tgeti(letters, pos + 1));
            MODIFY_TEXT_INPUT(
                TAB("letter", tget(args, "letter"), "text_table", tget(args, "text_table"), "pos", N(pos + 1), "delete", tget(args, "delete")));
        }
        return;
    }
    // note: a BACKSPACE at position 0 gets here with pos 0 and writes letters[0] = '', as the Lua (a key outside
    // the array that nothing reads)
    Val swapped_letter = tgeti(letters, pos);
    tseti(letters, pos, tget(args, "letter"));
    if (truthy(swapped_letter) && !veq(swapped_letter, S(""))) {
        MODIFY_TEXT_INPUT(TAB("letter", swapped_letter, "text_table", tget(args, "text_table"), "pos", N(pos + 1)));
    }
}

// button_callbacks.lua:1099
void TRANSPOSE_TEXT_INPUT(int amount) {
    int position_child = 0; // 0 as nil
    UIElement *hook = text_hook();
    Table *text = ttab(ttab(cfg_of(hook), "ref_table"), "text");
    for (int i = 1; i <= tlen(children_of(hook)); i++) {
        Node *child = tobji(children_of(hook), i);
        if (child->config) {
            if (veq(tget(child->config, "id"), S("position"))) {
                position_child = i;
                break;
            }
        }
    }
    if (!position_child) ERROR("attempt to perform arithmetic on local 'position_child' (a nil value)");

    // note: with amount 0 the Lua's dir is 0/0, never used: the loop does not run
    int dir = amount > 0 ? 1 : amount < 0 ? -1 : 0;

    while (amount != 0) {
        if (position_child + dir < 1 || position_child + dir >= tlen(children_of(hook))) break;
        bool real_letter = is_real_letter(tobji(children_of(hook), position_child + dir));
        SWAP(children_of(hook), N(position_child), N(position_child + dir));
        if (real_letter) amount = amount - dir;
        position_child = position_child + dir;
    }

    tsetn(text, "current_position", lmin(position_child - 1, str_len(vstr(text_ref_get(text)))));
    UIBox_recalculate(hook->UIBox);
    text_ref_set(text, S(GET_TEXT_FROM_INPUT()));
}

// ---------------------------------------------------------------- window settings
// button_callbacks.lua:1131
void FUNCS_can_apply_window_changes(UIElement *e) {
    bool can_apply = false;
    Table *QUEUED_CHANGE = ttab(G->SETTINGS, "QUEUED_CHANGE"), *WINDOW = ttab(G->SETTINGS, "WINDOW");
    if (QUEUED_CHANGE) {
        if (truthy(tget(QUEUED_CHANGE, "screenmode")) && !veq(tget(QUEUED_CHANGE, "screenmode"), tget(WINDOW, "screenmode"))) {
            can_apply = true;
        } else if (truthy(tget(QUEUED_CHANGE, "screenres"))) {
            can_apply = true;
        } else if (truthy(tget(QUEUED_CHANGE, "vsync"))) {
            can_apply = true;
        } else if (truthy(tget(QUEUED_CHANGE, "selected_display")) &&
                   !veq(tget(QUEUED_CHANGE, "selected_display"), tget(WINDOW, "selected_display"))) {
            can_apply = true;
        }
    }

    if (can_apply) {
        tsets(cfg_of(e), "button", "apply_window_changes");
        tset(cfg_of(e), "colour", P(G->C.RED));
    } else {
        tset(cfg_of(e), "button", NIL);
        tset(cfg_of(e), "colour", P(G->C.UI.BACKGROUND_INACTIVE));
    }
}

// button_callbacks.lua:1158
void FUNCS_apply_window_changes(bool _initial) {
    Table *WINDOW = ttab(G->SETTINGS, "WINDOW"), *QUEUED_CHANGE = ttab(G->SETTINGS, "QUEUED_CHANGE");
    // Set the screenmode setting from Windowed, Fullscreen or Borderless
    tset(WINDOW, "screenmode", vor(vor(tget(QUEUED_CHANGE, "screenmode"), tget(WINDOW, "screenmode")), S("Windowed")));

    // Set the monitor the window should be rendered to
    tset(WINDOW, "selected_display", vor(vor(tget(QUEUED_CHANGE, "selected_display"), tget(WINDOW, "selected_display")), N(1)));

    // Set the screen resolution
    Table *display = vtab(tgetv(ttab(WINDOW, "DISPLAYS"), tget(WINDOW, "selected_display")));
    Table *screenres = ttab(QUEUED_CHANGE, "screenres"), *screen_res = ttab(G->SETTINGS, "screen_res");
    tsett(display, "screen_res",
          TAB("w", vor(vor(tget(screenres, "w"), tget(screen_res, "w")), N(love_graphics_getWidth())), "h",
              vor(vor(tget(screenres, "h"), tget(screen_res, "h")), N(love_graphics_getHeight()))));

    // Set the vsync value, 0 is off 1 is on
    tset(WINDOW, "vsync", vor(vor(tget(QUEUED_CHANGE, "vsync"), tget(WINDOW, "vsync")), N(1)));

    // note: love.h's updateMode takes the size only: the flags table of the Lua (fullscreen, fullscreentype, vsync,
    // resizable, display, highdpi) has nowhere to go on the PS2's fixed video mode
    bool windowed = streq(tstr(QUEUED_CHANGE, "screenmode"), "Windowed");
    love_window_updateMode((int)(windowed ? love_graphics_getWidth() * 0.8 : tnum(ttab(display, "screen_res"), "w")),
                           (int)(windowed ? love_graphics_getHeight() * 0.8 : tnum(ttab(display, "screen_res"), "h")));
    tsett(G->SETTINGS, "QUEUED_CHANGE", tnew());
    if (!_initial) {
        love_resize(love_graphics_getWidth(), love_graphics_getHeight());
        Game_save_settings(G);
    }
    if (G->OVERLAY_MENU) {
        UIElement *tab_but = UIBox_get_UIE_by_ID(G->OVERLAY_MENU, S("tab_but_Video"), NULL);
        FUNCS_change_tab(tab_but);
    }
}

// button_callbacks.lua:1197
void INIT_COLLECTION_CARD_ALERTS(void) {
    for (int j = 1; j <= tlen(your_collection()); j++) {
        CardArea *area = tobji(your_collection(), j);
        for (int i = 1; CardArea_card(area, i); i++) {
            Card_update_alert(CardArea_card(area, i));
        }
    }
}

// ---------------------------------------------------------------- run setup
// button_callbacks.lua:1214
void FUNCS_RUN_SETUP_check_back(UIElement *e) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    if (!veq(S(viewed_back->name), tget(cfg_of(e), "id"))) {
        // removes the UI from the previously selected back and adds the new one

        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB("definition", T(Back_generate_UI(viewed_back, NULL, -1, -1, NULL)), "config",
                            T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("cm"), "parent", O(e))))));
        tset(cfg_of(e), "id", S(viewed_back->name));
    }
}

// button_callbacks.lua:1227
void FUNCS_RUN_SETUP_check_back_name(UIElement *e) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    if (tobj(cfg_of(e), "object") && !veq(S(viewed_back->name), tget(cfg_of(e), "id"))) {
        // removes the UI from the previously selected back and adds the new one

        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB(
                  "definition",
                  T(TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR))), "nodes",
                        T(ARR(T(TAB("n", N(UIT_O), "config",
                                    T(TAB("id", S(viewed_back->name), "func", S("RUN_SETUP_check_back_name"), "object",
                                          O(DynaText_new(TAB("string", S(Back_get_name(viewed_back)), "maxw", N(4), "colours",
                                                             T(ARR(P(G->C.WHITE))), "shadow", B(true), "bump", B(true), "scale", N(0.5),
                                                             "pop_in", N(0), "silent", B(true)))))))))))),
                  "config", T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("cm"), "parent", O(e))))));
        tset(cfg_of(e), "id", S(viewed_back->name));
    }
}

// button_callbacks.lua:1242
void FUNCS_RUN_SETUP_check_stake(UIElement *e) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    if (!veq(S(viewed_back->name), tget(cfg_of(e), "id"))) {
        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB("definition", T(UIDEF_stake_option(tstr(G->SETTINGS, "current_setup"))), "config",
                            T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("tmi"), "parent", O(e))))));
        tset(cfg_of(e), "id", S(viewed_back->name));
    }
}

// G.viewed_stake as a value (the member is 0 as nil)
static inline Val viewed_stake(void) { return G->viewed_stake ? N(G->viewed_stake) : NIL; }

// button_callbacks.lua:1253
void FUNCS_RUN_SETUP_check_stake2(UIElement *e) {
    if (!veq(viewed_stake(), tget(cfg_of(e), "id"))) {
        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB("definition", T(UIDEF_viewed_stake_option()), "config",
                            T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("cm"), "parent", O(e))))));
        tset(cfg_of(e), "id", viewed_stake());
    }
}

// button_callbacks.lua:1264
void FUNCS_change_viewed_collab(Table *args) { G->viewed_collab = tstr(args, "to_val"); }

// button_callbacks.lua:1268
void FUNCS_CREDITS_check_collab(UIElement *e) {
    if (!veq(S(G->viewed_collab), tget(cfg_of(e), "id"))) {
        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB("definition", T(UIDEF_viewed_collab_option()), "config",
                            T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("cm"), "parent", O(e))))));
        tset(cfg_of(e), "id", S(G->viewed_collab));
    }
}

// button_callbacks.lua:1279
void FUNCS_RUN_SETUP_check_back_stake_column(UIElement *e) {
    Back *viewed_back = tobj(G->GAME, "viewed_back");
    if (!veq(S(viewed_back->name), tget(cfg_of(e), "id"))) {
        // removes the UI from the previously selected back and adds the new one
        CALL(tobj(cfg_of(e), "object"), remove);
        tseto(cfg_of(e), "object",
              UIBox_new(TAB("definition", T(UIDEF_deck_stake_column(tstr(ttab(viewed_back->effect, "center"), "key"))), "config",
                            T(TAB("offset", T(TAB("x", N(0), "y", N(0))), "align", S("cm"), "parent", O(e))))));
        tset(cfg_of(e), "id", S(viewed_back->name));
    }
}

// button_callbacks.lua:1291
void FUNCS_RUN_SETUP_check_back_stake_highlight(UIElement *e) {
    if (veq(viewed_stake(), tget(cfg_of(e), "id")) && tnum(cfg_of(e), "outline") < 0.1) {
        tsetn(cfg_of(e), "outline", 0.8);
    } else if (!veq(viewed_stake(), tget(cfg_of(e), "id")) && tnum(cfg_of(e), "outline") > 0.1) {
        tsetn(cfg_of(e), "outline", 0);
    }
}
