// functions/misc_functions.lua:627-1258: nuGC .. set_hand_usage.
#include "all.h"

// G.PROFILES[G.SETTINGS.profile]
static Table *cur_profile(void) { return ttabi(G->PROFILES, (int)tnum(G->SETTINGS, "profile")); }

// math.log(x, b) as LuaJIT 2.1 computes it: log2(x) * (1/log2(b)), not joined into a division.
// note: spec trap 19: this form was not verified against the shipped lua51.dll.
static num lj_log(num x, num b) {
    volatile num y = 1.0 / log2(b);
    return log2(x) * y;
}

// misc_functions.lua:645
void nuGC(num time_budget, num memory_ceiling, bool disable_otherwise) {
    if (time_budget == 0) time_budget = 3e-4;
    if (memory_ceiling == 0) memory_ceiling = 300;
    // note: collectgarbage("step" / "count" / "collect" / "stop"): there is no collector (docs/conventions.md, Memory)
}

// misc_functions.lua:669
void add_to_drawhash(void *obj) {
    if (obj) tinsert(G->DRAW_HASH, O(obj));
}

// misc_functions.lua:675
// note: `C1[i] or 0.5` cannot be nil for a float[4]
float *mix_colours(const float *C1, const float *C2, float proportionC1, float *out) {
    if (!out) out = malloc(sizeof(Colour));
    float r = C1[0] * proportionC1 + C2[0] * (1 - proportionC1);
    float g = C1[1] * proportionC1 + C2[1] * (1 - proportionC1);
    float b = C1[2] * proportionC1 + C2[2] * (1 - proportionC1);
    float a = C1[3] * proportionC1 + C2[3] * (1 - proportionC1);
    out[0] = r, out[1] = g, out[2] = b, out[3] = a; // (out may be C1 or C2)
    return out;
}

// misc_functions.lua:684
num mod_chips(num _chips) {
    if (truthy(tget(ttab(G->GAME, "modifiers"), "chips_dollar_cap"))) {
        _chips = lmin(_chips, lmax(tnum(G->GAME, "dollars"), 0));
    }
    return _chips;
}

// misc_functions.lua:691
num mod_mult(num _mult) { return _mult; }

// misc_functions.lua:695
void play_sound(const char *sound_code, float per, float vol) {
    if (G->F_MUTE) return;
    if (sound_code && tnum(ttab(G->SETTINGS, "SOUND"), "volume") > 0.001) {
        Table *a = ttab(G->ARGS, "play_sound");
        if (!a) a = tnew(), tsett(G->ARGS, "play_sound", a);
        tsets(a, "type", "sound");
        tsetn(a, "time", G->TIMERS.REAL);
        tset(a, "crt", tget(ttab(G->SETTINGS, "GRAPHICS"), "crt"));
        tsets(a, "sound_code", sound_code);
        tset(a, "per", per < 0 ? NIL : N(per));
        tset(a, "vol", vol < 0 ? NIL : N(vol));
        tsetn(a, "pitch_mod", G->PITCH_MOD);
        tsetn(a, "state", G->STATE);
        tset(a, "music_control", tget(G->SETTINGS, "music_control"));
        tset(a, "sound_settings", tget(G->SETTINGS, "SOUND"));
        tset(a, "splash_vol", G->SPLASH_VOL);
        tsetb(a, "overlay_menu", G->OVERLAY_MENU != NULL);
        // note: G.F_SOUND_THREAD is always the port's path (io.h): the channel push is sound_manager_request; the
        // main thread's PLAY_SOUND of misc_functions.lua:1892 does not exist
        sound_manager_request(a);
    }
}

// misc_functions.lua:751-754 the four volfuncs of G.ARGS.ambient_sounds. ctx: the dt of the first modulate_sound
// call (the closures are made once and capture it); everything else is read live.
static Val volfunc_ambientFire2(void *ctx, int argc, Val *argv) {
    num dt = *(num *)ctx, _prev_volume = vnum(argv[0]);
    num flames = tnum(ttab(G->ARGS, "score_intensity"), "flames");
    return N(_prev_volume * (1 - dt) + dt * 0.9 * ((flames > 0.3) ? 1 : flames / 0.3));
}
static Val volfunc_ambientFire1(void *ctx, int argc, Val *argv) {
    num dt = *(num *)ctx, _prev_volume = vnum(argv[0]);
    num flames = tnum(ttab(G->ARGS, "score_intensity"), "flames");
    return N(_prev_volume * (1 - dt) + dt * 0.8 * ((flames > 0.3) ? (flames - 0.3) / 0.7 : 0));
}
static Val volfunc_ambientFire3(void *ctx, int argc, Val *argv) {
    num dt = *(num *)ctx, _prev_volume = vnum(argv[0]);
    Table *chip_flames = ttab(G->ARGS, "chip_flames"), *mult_flames = ttab(G->ARGS, "mult_flames");
    return N(_prev_volume * (1 - dt) +
             dt * 0.4 * ((chip_flames ? tnum(chip_flames, "change") : 0) + (mult_flames ? tnum(mult_flames, "change") : 0)));
}
static Val volfunc_ambientOrgan1(void *ctx, int argc, Val *argv) {
    num dt = *(num *)ctx, _prev_volume = vnum(argv[0]);
    return N(_prev_volume * (1 - dt) + dt * 0.6 * (tnum(ttab(G->SETTINGS, "SOUND"), "music_volume") + 100) / 200 *
                                           vnum(tget(ttab(G->ARGS, "score_intensity"), "organ")));
}

// misc_functions.lua:719
void modulate_sound(float dt) {
    // volume of the splash screen is set here
    G->SPLASH_VOL = N(2 * (num)dt * (G->STATE == STATES_SPLASH ? 1 : 0) +
                      (truthy(G->SPLASH_VOL) ? vnum(G->SPLASH_VOL) : 1) * (1 - 2 * (num)dt));

    // Control the music here
    Blind *blind = tobj(G->GAME, "blind");
    const char *desired_track =
        G->video_soundtrack ? G->video_soundtrack
        : (G->STATE == STATES_SPLASH) ? ""
        : (G->booster_pack_sparkles && !((Node *)G->booster_pack_sparkles)->REMOVED) ? "music2"
        : (G->booster_pack_meteors && !((Node *)G->booster_pack_meteors)->REMOVED) ? "music3"
        : (G->booster_pack && !((Node *)G->booster_pack)->REMOVED) ? "music2"
        : (G->shop && !((Node *)G->shop)->REMOVED) ? "music4"
        : (blind && blind->boss) ? "music5"
        : "music1";

    // (G.PITCH_MOD or 1: the member is 1 from set_globals)
    G->PITCH_MOD = G->PITCH_MOD * (1 - dt) + dt * ((!G->normal_music_speed && G->STATE == STATES_GAME_OVER) ? 0.5f : 1);

    // For ambient sound control
    Table *AC = ttab(G->SETTINGS, "ambient_control");
    if (!AC) AC = tnew(), tsett(G->SETTINGS, "ambient_control", AC);
    Table *si = ttab(G->ARGS, "score_intensity");
    if (!si) si = tnew(), tsett(G->ARGS, "score_intensity", si);
    Table *current_hand = ttab(ttab(G->GAME, "current_round"), "current_hand");
    Val chips = tget(current_hand, "chips"), mult_ = tget(current_hand, "mult");
    if (chips.t != V_NUM || mult_.t != V_NUM) {
        tsetn(si, "earned_score", 0);
    } else {
        tsetn(si, "earned_score", chips.n * mult_.n);
    }
    tsetn(si, "required_score", blind ? blind->chips : 0);
    Table *chip_flames = ttab(G->ARGS, "chip_flames");
    tsetn(si, "flames",
          lmin(1, (G->STAGE == STAGES_RUN ? 1 : 0) *
                      (chip_flames ? (tnum(chip_flames, "real_intensity") + tnum(chip_flames, "change")) : 0) / 10));
    num earned_score = tnum(si, "earned_score"), required_score = tnum(si, "required_score");
    tset(si, "organ",
         truthy(G->video_organ) ? G->video_organ
                                : N(required_score > 0 ? lmax(lmin(0.4, 0.1 * lj_log(earned_score / (required_score + 1), 5)), 0.) : 0));

    Table *ambient_sounds = ttab(G->ARGS, "ambient_sounds");
    if (!ambient_sounds) {
        num *dt0 = NEWCTX(num); // the upvalue dt of the four closures: this call's, for ever
        *dt0 = dt;
        ambient_sounds = TAB("ambientFire2", T(TAB("volfunc", FN(volfunc_ambientFire2, dt0))),
                             "ambientFire1", T(TAB("volfunc", FN(volfunc_ambientFire1, dt0))),
                             "ambientFire3", T(TAB("volfunc", FN(volfunc_ambientFire3, dt0))),
                             "ambientOrgan1", T(TAB("volfunc", FN(volfunc_ambientOrgan1, dt0))));
        tsett(G->ARGS, "ambient_sounds", ambient_sounds);
    }

    // note: pairs() order (hash order in the Lua, insertion order here); the four entries are independent
    for (TIter it = titer(ambient_sounds); tnext(&it);) {
        const char *k = it.key.s;
        Table *v = it.val.tab;
        Table *ACk = ttab(AC, k);
        if (!ACk) ACk = tnew(), tsett(AC, k, ACk);
        tsetn(ACk, "per", streq(k, "ambientOrgan1") ? 0.7 : streq(k, "ambientFire1") ? 1.1 : streq(k, "ambientFire2") ? 1.05 : 1);
        Val vol = tget(ACk, "vol");
        if (!truthy(G->video_organ) && G->STATE == STATES_SPLASH) {
            tsetn(ACk, "vol", 0);
        } else if (truthy(vol)) {
            Val r = fcall(tget(v, "volfunc"), 1, &vol);
            tset(ACk, "vol", truthy(r) ? r : N(0));
        } else {
            tsetn(ACk, "vol", 0);
        }
    }

    Table *push = ttab(G->ARGS, "push");
    if (!push) push = tnew(), tsett(G->ARGS, "push", push);
    tsets(push, "type", "modulate");
    tsetn(push, "pitch_mod", G->PITCH_MOD);
    tsetn(push, "state", G->STATE);
    tsetn(push, "time", G->TIMERS.REAL);
    tsetn(push, "dt", dt);
    tsets(push, "desired_track", desired_track);
    tset(push, "sound_settings", tget(G->SETTINGS, "SOUND"));
    tset(push, "splash_vol", G->SPLASH_VOL);
    tsetb(push, "overlay_menu", G->OVERLAY_MENU != NULL);
    tset(push, "ambient_control", tget(G->SETTINGS, "ambient_control"));

    // note: as play_sound: the channel push (it runs MODULATE and then AMBIENT, io.h)
    sound_manager_request(push);
}

// misc_functions.lua:782
int count_of_suit(CardArea *area, const char *suit) {
    int num_ = 0;
    for (TIter it = titer(area->cards); tnext(&it);) {
        Card *c = it.val.obj;
        if (veq(tget(c->base, "suit"), S(suit))) num_ = num_ + 1;
    }
    return num_;
}

// misc_functions.lua:792
void prep_draw(void *moveable_, float scale, float rotate, const Vec2 *offset) {
    Moveable *moveable = moveable_;
    love_graphics_push();
    love_graphics_scale(G->TILESCALE * G->TILESIZE, G->TILESCALE * G->TILESIZE);
    // note: layered_parallax is a Vec2 by value, never nil, so the Lua's fallback to the parent's does not exist. The
    // only nil ones are a card's front / back / center (card.lua:68-70), whose parent's (the card's, a copy of its
    // area's) is {0, 0}, which is what those sprites hold in C (card_1.c).
    love_graphics_translate(moveable->VT.x + moveable->VT.w / 2 + (offset ? offset->x : 0) + moveable->layered_parallax.x,
                            moveable->VT.y + moveable->VT.h / 2 + (offset ? offset->y : 0) + moveable->layered_parallax.y);
    // note: rotate 0 stands for nil (header): skipping rotate(VT.r + 0) with VT.r == 0 is the same transform
    if (moveable->VT.r != 0 || moveable->juice.on || rotate != 0) love_graphics_rotate(moveable->VT.r + rotate);
    love_graphics_translate(-scale * moveable->VT.w * (moveable->VT.scale) / 2, -scale * moveable->VT.h * (moveable->VT.scale) / 2);
    love_graphics_scale(moveable->VT.scale * scale, moveable->VT.scale * scale);
}

// misc_functions.lua:805
void get_chosen_triangle_from_rect(float x, float y, float w, float h, bool vert, float out[6]) {
    float scale = 2;
    if (vert) {
        x = x + fminf(0.6f * sinf(wrap_angle((float)G->TIMERS.REAL * 9)) * scale + 0.2f, 0);
        out[0] = x - 3.5f * scale, out[1] = y + h / 2 - 1.5f * scale;
        out[2] = x - 0.5f * scale, out[3] = y + h / 2 + 0;
        out[4] = x - 3.5f * scale, out[5] = y + h / 2 + 1.5f * scale;
    } else {
        y = y + fminf(0.6f * sinf(wrap_angle((float)G->TIMERS.REAL * 9)) * scale + 0.2f, 0);
        out[0] = x + w / 2 - 1.5f * scale, out[1] = y - 4 * scale;
        out[2] = x + w / 2 + 0, out[3] = y - 1.1f * scale;
        out[4] = x + w / 2 + 1.5f * scale, out[5] = y - 4 * scale;
    }
}

// misc_functions.lua:824
void point_translate(Vec2 *_T, Vec2 delta) {
    _T->x = _T->x + delta.x;
    _T->y = _T->y + delta.y;
}

// misc_functions.lua:829
void point_rotate(Vec2 *_T, float angle) {
    float _cos = cosf(angle + (float)M_PI / 2), _sin = sinf(angle + (float)M_PI / 2), _ox = _T->x, _oy = _T->y;
    _T->x = -_oy * _cos + _ox * _sin;
    _T->y = _oy * _sin + _ox * _cos;
}

// misc_functions.lua:835
float *lighten(const float *colour, float percent, float *no_tab) {
    float *out = no_tab ? no_tab : malloc(sizeof(Colour));
    float a = colour[3];
    out[0] = colour[0] * (1 - percent) + percent;
    out[1] = colour[1] * (1 - percent) + percent;
    out[2] = colour[2] * (1 - percent) + percent;
    out[3] = a;
    return out;
}

// misc_functions.lua:851
float *darken(const float *colour, float percent, float *no_tab) {
    float *out = no_tab ? no_tab : malloc(sizeof(Colour));
    float a = colour[3];
    out[0] = colour[0] * (1 - percent);
    out[1] = colour[1] * (1 - percent);
    out[2] = colour[2] * (1 - percent);
    out[3] = a;
    return out;
}

// misc_functions.lua:867
float *adjust_alpha(const float *colour, float new_alpha, float *no_tab) {
    float *out = no_tab ? no_tab : malloc(sizeof(Colour));
    out[0] = colour[0];
    out[1] = colour[1];
    out[2] = colour[2];
    out[3] = new_alpha;
    return out;
}

// misc_functions.lua:893
static bool ev_alert_no_space_sound(void *ctx) {
    play_sound("tarot2", 0.76f, 0.4f);
    return true;
}
// misc_functions.lua:898
static bool ev_alert_no_space_unlock(void *ctx) {
    tset(G->CONTROLLER->locks, "no_space", NIL);
    return true;
}

// misc_functions.lua:883
void alert_no_space(Card *card, CardArea *area) {
    tsetb(G->CONTROLLER->locks, "no_space", true);
    attention_text(TAB("scale", N(0.9), "text", localize(S("k_no_space_ex"), NULL), "hold", N(0.9), "align", S("cm"),
                       "cover", O(area), "cover_padding", N(0.1), "cover_colour", P(adjust_alpha(G->C.BLACK, 0.7f, NULL))));
    CALL(card, juice_up, 0.3f, N(0.2));
    for (int i = 1; i <= tlen(area->cards); i++) {
        CALL(CardArea_card(area, i), juice_up, 0.15f, NIL);
    }
    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = 0.06 * tnum(G->SETTINGS, "GAMESPEED"), .not_blockable = true,
                               .not_blocking = true, .func = ev_alert_no_space_sound}),
          NULL, false);
    play_sound("tarot2", 1, 0.4f);

    E_add(G->E_MANAGER,
          Event_new((EventCfg){.trigger = "after", .delay = 0.5 * tnum(G->SETTINGS, "GAMESPEED"), .not_blockable = true,
                               .not_blocking = true, .func = ev_alert_no_space_unlock}),
          NULL, false);
}

// misc_functions.lua:903
Table *find_joker(const char *name, bool non_debuff) {
    Table *jokers = tnew();
    if (!G->jokers || !G->jokers->cards) return tnew();
    for (TIter it = titer(G->jokers->cards); tnext(&it);) {
        Card *v = it.val.obj;
        if (it.val.t == V_OBJ && streq(tstr(v->ability, "name"), name) && (non_debuff || !v->debuff)) {
            tinsert(jokers, O(v));
        }
    }
    // (the Lua indexes G.consumeables without a check, too)
    for (TIter it = titer(G->consumeables->cards); tnext(&it);) {
        Card *v = it.val.obj;
        if (it.val.t == V_OBJ && streq(tstr(v->ability, "name"), name) && (non_debuff || !v->debuff)) {
            tinsert(jokers, O(v));
        }
    }
    return jokers;
}

// misc_functions.lua:919
num get_blind_amount(num ante) {
    num k = 0.75;
    Val scaling = tget(ttab(G->GAME, "modifiers"), "scaling");
    if (!truthy(scaling) || veq(scaling, N(1))) {
        static const num amounts[8] = {300, 800, 2000, 5000, 11000, 20000, 35000, 50000};
        if (ante < 1) return 100;
        if (ante <= 8) return amounts[(int)ante - 1];
        num a = amounts[7], b = 1.6, c = ante - 8, d = 1 + 0.2 * (ante - 8);
        num amount = floor(a * pow(b + pow(k * c, d), c));
        amount = amount - lmod(amount, pow(10, floor(log10(amount) - 1)));
        return amount;
    } else if (veq(scaling, N(2))) {
        static const num amounts[8] = {300, 900, 2600, 8000, 20000, 36000, 60000, 100000};
        if (ante < 1) return 100;
        if (ante <= 8) return amounts[(int)ante - 1];
        num a = amounts[7], b = 1.6, c = ante - 8, d = 1 + 0.2 * (ante - 8);
        num amount = floor(a * pow(b + pow(k * c, d), c));
        amount = amount - lmod(amount, pow(10, floor(log10(amount) - 1)));
        return amount;
    } else if (veq(scaling, N(3))) {
        static const num amounts[8] = {300, 1000, 3200, 9000, 25000, 60000, 110000, 200000};
        if (ante < 1) return 100;
        if (ante <= 8) return amounts[(int)ante - 1];
        num a = amounts[7], b = 1.6, c = ante - 8, d = 1 + 0.2 * (ante - 8);
        num amount = floor(a * pow(b + pow(k * c, d), c));
        amount = amount - lmod(amount, pow(10, floor(log10(amount) - 1)));
        return amount;
    }
    // note: the Lua returns nil here and the caller's arithmetic fails (header)
    ERROR("get_blind_amount: G.GAME.modifiers.scaling is not nil, 1, 2 or 3");
}

// misc_functions.lua:956
const char *number_format(Val _num) {
    if (G->E_SWITCH_POINT == 0) G->E_SWITCH_POINT = 100000000000;
    if (!truthy(_num)) return "";
    if (_num.t != V_NUM) return _num.t == V_STR ? _num.s : tostring(_num); // note: the Lua returns the value itself
    num n = _num.n;
    if (n >= G->E_SWITCH_POINT) {
        const char *x = fmt("%.4g", n);
        num xn = tonumber(S(x), NULL); // the Lua's arithmetic on the string x
        num fac = floor(lj_log(xn, 10));
        return fmt("%s%s%s", fmt("%.3f", xn / pow(10, fac)), "e", number_tostring(fac));
    }
    const char *s = n != floor(n) ? (n >= 100 ? fmt("%.0f", n) : n >= 10 ? fmt("%.1f", n) : fmt("%.2f", n)) : fmt("%.0f", n);
    // :reverse():gsub("(%d%d%d)", "%1,"):gsub(",$", ""):reverse()
    int len = (int)strlen(s), o = 0;
    char rev[512], out[700];
    if (len > 500) ERROR("number_format: number too long");
    for (int i = 0; i < len; i++) rev[i] = s[len - 1 - i];
    rev[len] = 0;
    for (int i = 0; i < len;) {
        if (i + 2 < len && rev[i] >= '0' && rev[i] <= '9' && rev[i + 1] >= '0' && rev[i + 1] <= '9' && rev[i + 2] >= '0' &&
            rev[i + 2] <= '9') {
            out[o++] = rev[i], out[o++] = rev[i + 1], out[o++] = rev[i + 2], out[o++] = ',';
            i += 3;
        } else {
            out[o++] = rev[i++];
        }
    }
    if (o > 0 && out[o - 1] == ',') o--;
    for (int i = 0; i < o / 2; i++) {
        char c = out[i];
        out[i] = out[o - 1 - i], out[o - 1 - i] = c;
    }
    out[o] = 0;
    return str(out);
}

// misc_functions.lua:967
float score_number_scale(float scale, Val amt) {
    if (G->E_SWITCH_POINT == 0) G->E_SWITCH_POINT = 100000000000;
    if (amt.t != V_NUM) return 0.7f * scale;
    if (amt.n >= G->E_SWITCH_POINT) {
        return 0.7f * scale;
    } else if (amt.n >= 1000000) {
        return (float)(14 * 0.75 / (floor(log(amt.n)) + 4) * scale);
    } else {
        return 0.75f * scale;
    }
}

// misc_functions.lua:979 copy_table: static inline in misc_functions.h (tcopy)

// misc_functions.lua:994
void send_score(num _score) {
    if (G->F_HTTP_SCORES && truthy(tget(G->SETTINGS, "COMP")) && G->F_STREAMER_EVENT) {
        // G.HTTP_MANAGER.out_channel:push(...): the HTTP manager and Steam are not translated (docs/conventions.md)
    }
}

// misc_functions.lua:1006
void send_name(void) {
    if (G->F_HTTP_SCORES && truthy(tget(G->SETTINGS, "COMP")) && G->F_STREAMER_EVENT) {
        // G.HTTP_MANAGER.out_channel:push(...): the HTTP manager and Steam are not translated (docs/conventions.md)
    }
}

// misc_functions.lua:1017
// (amt is always a number here: the Lua's first line, `if not amt or type(amt) ~= 'number'`, cannot be taken)
void check_and_set_high_score(const char *score, num amt) {
    Table *round_score = ttab(ttab(G->GAME, "round_scores"), score);
    if (round_score && floor(amt) > tnum(round_score, "amt")) {
        tsetn(round_score, "amt", floor(amt));
    }
    if (tbool(G->GAME, "seeded")) return;
    Table *COMP = ttab(G->SETTINGS, "COMP");
    if (streq(score, "hand") && COMP && (!truthy(tget(COMP, "score")) || tnum(COMP, "score") < floor(amt))) {
        tsetn(COMP, "score", amt);
        send_score(floor(amt));
    }
    Table *high_score = ttab(ttab(cur_profile(), "high_scores"), score);
    if (high_score && floor(amt) > tnum(high_score, "amt")) {
        if (ttab(ttab(G->GAME, "round_scores"), score)) tsetb(ttab(ttab(G->GAME, "round_scores"), score), "high_score", true);
        tsetn(high_score, "amt", floor(amt));
        Game_save_settings(G);
    }
}

// {count = 1, order = order, wins = {}, losses = {}}
static Table *new_usage(Val order) { return TAB("count", N(1), "order", order, "wins", T(tnew()), "losses", T(tnew())); }

// misc_functions.lua:1034
void set_joker_usage(void) {
    for (TIter it = titer(G->jokers->cards); tnext(&it);) {
        Card *v = it.val.obj;
        const char *center_key = tstr(v->moveable.node.config, "center_key");
        if (center_key && streq(tstr(v->ability, "set"), "Joker")) {
            Table *joker_usage = ttab(cur_profile(), "joker_usage");
            Table *u = ttab(joker_usage, center_key);
            if (u) {
                tsetn(u, "count", tnum(u, "count") + 1);
            } else {
                tsett(joker_usage, center_key, new_usage(tget(ttab(v->moveable.node.config, "center"), "order")));
            }
        }
    }
    Game_save_settings(G);
}

// misc_functions.lua:1047
void set_joker_win(void) {
    for (TIter it = titer(G->jokers->cards); tnext(&it);) {
        Card *v = it.val.obj;
        const char *center_key = tstr(v->moveable.node.config, "center_key");
        if (center_key && streq(tstr(v->ability, "set"), "Joker")) {
            Table *joker_usage = ttab(cur_profile(), "joker_usage");
            if (!ttab(joker_usage, center_key)) {
                tsett(joker_usage, center_key, new_usage(tget(ttab(v->moveable.node.config, "center"), "order")));
            }
            Table *u = ttab(joker_usage, center_key);
            if (u) {
                if (!ttab(u, "wins")) tsett(u, "wins", tnew());
                Table *wins = ttab(u, "wins");
                Val stake = tget(G->GAME, "stake");
                Val w = tgetv(wins, stake);
                tsetv(wins, stake, N((truthy(w) ? vnum(w) : 0) + 1));
            }
        }
    }
    Game_save_settings(G);
}

// misc_functions.lua:1060
Val get_joker_win_sticker(Table *_center, bool index) {
    Table *u = ttab(ttab(cur_profile(), "joker_usage"), tstr(_center, "key"));
    if (u && ttab(u, "wins")) {
        num _w = 0;
        for (TIter it = titer(ttab(u, "wins")); tnext(&it);) {
            _w = lmax(vnum(it.key), _w);
        }
        if (index) return N(_w);
        if (_w > 0) return tgetv(G->sticker_map, N(_w));
    }
    if (index) return N(0);
    return NIL;
}

// misc_functions.lua:1073
void set_joker_loss(void) {
    for (TIter it = titer(G->jokers->cards); tnext(&it);) {
        Card *v = it.val.obj;
        const char *center_key = tstr(v->moveable.node.config, "center_key");
        if (center_key && streq(tstr(v->ability, "set"), "Joker")) {
            Table *u = ttab(ttab(cur_profile(), "joker_usage"), center_key);
            if (u) {
                if (!ttab(u, "losses")) tsett(u, "losses", tnew());
                Table *losses = ttab(u, "losses");
                Val stake = tget(G->GAME, "stake");
                Val l = tgetv(losses, stake);
                tsetv(losses, stake, N((truthy(l) ? vnum(l) : 0) + 1));
            }
        }
    }
    Game_save_settings(G);
}

// G.GAME.selected_back.effect.center when every link of selected_back.effect.center.key is there, else NULL
static Table *selected_back_center(void) {
    Back *selected_back = tobj(G->GAME, "selected_back");
    Table *center = selected_back && selected_back->effect ? ttab(selected_back->effect, "center") : NULL;
    return center && truthy(tget(center, "key")) ? center : NULL;
}

// misc_functions.lua:1085
void set_deck_usage(void) {
    Table *center = selected_back_center();
    if (center) {
        const char *deck_key = tstr(center, "key");
        Table *deck_usage = ttab(cur_profile(), "deck_usage");
        Table *u = ttab(deck_usage, deck_key);
        if (u) {
            tsetn(u, "count", tnum(u, "count") + 1);
        } else {
            tsett(deck_usage, deck_key, new_usage(tget(center, "order")));
        }
        Game_save_settings(G);
    }
}

// misc_functions.lua:1097
void set_deck_win(void) {
    Table *center = selected_back_center();
    if (center) {
        const char *deck_key = tstr(center, "key");
        Table *deck_usage = ttab(cur_profile(), "deck_usage");
        if (!ttab(deck_usage, deck_key)) tsett(deck_usage, deck_key, new_usage(tget(center, "order")));
        Table *u = ttab(deck_usage, deck_key);
        if (u) {
            Table *wins = ttab(u, "wins");
            if (!wins) ERROR("attempt to index field 'wins' (a nil value)");
            Val stake = tget(G->GAME, "stake");
            Val w = tgetv(wins, stake);
            tsetv(wins, stake, N((truthy(w) ? vnum(w) : 0) + 1));
            for (int i = 1; i <= vnum(stake); i++) {
                Val wi = tgeti(wins, i);
                tseti(wins, i, truthy(wi) ? wi : N(1));
            }
        }
        set_challenge_unlock();
        Game_save_settings(G);
    }
}

// misc_functions.lua:1112
void set_challenge_unlock(void) {
    Table *profile = cur_profile();
    if (tbool(profile, "all_unlocked")) return;
    if (tbool(profile, "challenges_unlocked")) {
        num _ch_comp = 0, _ch_tot = tlen(G->CHALLENGES);
        for (int k = 1; truthy(tgeti(G->CHALLENGES, k)); k++) {
            Table *v = ttabi(G->CHALLENGES, k);
            Val id = tget(v, "id");
            if (truthy(id) && truthy(tgetv(ttab(ttab(profile, "challenge_progress"), "completed"), id))) {
                _ch_comp = _ch_comp + 1;
            }
        }
        tsetn(profile, "challenges_unlocked", lmin(_ch_tot, _ch_comp + 5));
    } else {
        num deck_wins = 0;
        for (TIter it = titer(ttab(profile, "deck_usage")); tnext(&it);) {
            Table *v = it.val.tab;
            if (ttab(v, "wins") && truthy(tgeti(ttab(v, "wins"), 1))) {
                deck_wins = deck_wins + 1;
            }
        }
        if (deck_wins >= G->CHALLENGE_WINS && !tbool(profile, "challenges_unlocked")) {
            tsetn(profile, "challenges_unlocked", 5);
            notify_alert("b_challenge", "Back");
        }
    }
}

// misc_functions.lua:1136
num get_deck_win_stake(const char *_deck_key, num *_w_low_out) {
    Table *deck_usage = ttab(cur_profile(), "deck_usage");
    if (!_deck_key) {
        num _w = 0, _w_low = 0;
        bool have_w_low = false; // _w_low = nil
        num deck_count = 0;
        // note: pairs() order: _w_low ends as the running _w after the first deck visited (hash order in the Lua,
        // insertion order here)
        for (TIter it = titer(deck_usage); tnext(&it);) {
            Table *deck = it.val.tab;
            bool deck_won_with = false;
            if (!ttab(deck, "wins")) ERROR("bad argument #1 to 'pairs' (table expected, got nil)");
            for (TIter jt = titer(ttab(deck, "wins")); tnext(&jt);) {
                deck_won_with = true;
                _w = lmax(vnum(jt.key), _w);
            }
            if (deck_won_with) deck_count = deck_count + 1;
            _w_low = have_w_low ? lmin(_w_low, _w) : _w;
            have_w_low = true;
        }
        if (_w_low_out) *_w_low_out = (deck_count >= tlen(ttab(G->P_CENTER_POOLS, "Back")) && have_w_low) ? _w_low : 0;
        return _w;
    }
    Table *u = ttab(deck_usage, _deck_key);
    if (u && ttab(u, "wins")) {
        num _w = 0;
        for (TIter it = titer(ttab(u, "wins")); tnext(&it);) {
            _w = lmax(vnum(it.key), _w);
        }
        return _w;
    }
    return 0;
}

// misc_functions.lua:1162
const char *get_deck_win_sticker(Table *_center) {
    Table *u = ttab(ttab(cur_profile(), "deck_usage"), tstr(_center, "key"));
    if (u && ttab(u, "wins")) {
        num _w = -1;
        for (TIter it = titer(ttab(u, "wins")); tnext(&it);) {
            _w = lmax(vnum(it.key), _w);
        }
        if (_w > 0) return vstr(tgetv(G->sticker_map, N(_w)));
    }
    return NULL;
}

// misc_functions.lua:1173
void set_deck_loss(void) {
    Table *center = selected_back_center();
    if (center) {
        const char *deck_key = tstr(center, "key");
        Table *deck_usage = ttab(cur_profile(), "deck_usage");
        if (!ttab(deck_usage, deck_key)) tsett(deck_usage, deck_key, new_usage(tget(center, "order")));
        Table *u = ttab(deck_usage, deck_key);
        if (u) {
            Table *losses = ttab(u, "losses");
            if (!losses) ERROR("attempt to index field 'losses' (a nil value)");
            Val stake = tget(G->GAME, "stake");
            Val l = tgetv(losses, stake);
            tsetv(losses, stake, N((truthy(l) ? vnum(l) : 0) + 1));
        }
        Game_save_settings(G);
    }
}

// misc_functions.lua:1218 captures card; reads card.config.center_key when it runs
static bool ev_last_tarot_planet_inner(void *ctx) {
    Card *card = ctx;
    tset(G->GAME, "last_tarot_planet", tget(card->moveable.node.config, "center_key"));
    return true;
}
// misc_functions.lua:1215
static bool ev_last_tarot_planet(void *ctx) {
    E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_last_tarot_planet_inner, .ctx = ctx}), NULL, false);
    return true;
}

// misc_functions.lua:1184
void set_consumeable_usage(Card *card) {
    Table *config = card->moveable.node.config;
    const char *center_key = tstr(config, "center_key");
    if (center_key && truthy(tget(card->ability, "consumeable"))) {
        Table *center = ttab(config, "center");
        Table *p_usage = ttab(cur_profile(), "consumeable_usage");
        if (ttab(p_usage, center_key)) {
            tsetn(ttab(p_usage, center_key), "count", tnum(ttab(p_usage, center_key), "count") + 1);
        } else {
            tsett(p_usage, center_key, TAB("count", N(1), "order", tget(center, "order")));
        }
        Table *g_usage = ttab(G->GAME, "consumeable_usage");
        if (ttab(g_usage, center_key)) {
            tsetn(ttab(g_usage, center_key), "count", tnum(ttab(g_usage, center_key), "count") + 1);
        } else {
            tsett(g_usage, center_key, TAB("count", N(1), "order", tget(center, "order"), "set", tget(card->ability, "set")));
        }
        if (!ttab(G->GAME, "consumeable_usage_total")) {
            tsett(G->GAME, "consumeable_usage_total",
                  TAB("tarot", N(0), "planet", N(0), "spectral", N(0), "tarot_planet", N(0), "all", N(0)));
        }
        Table *total = ttab(G->GAME, "consumeable_usage_total");
        const char *set = tstr(center, "set");
        if (streq(set, "Tarot")) {
            tsetn(total, "tarot", tnum(total, "tarot") + 1);
            tsetn(total, "tarot_planet", tnum(total, "tarot_planet") + 1);
        } else if (streq(set, "Planet")) {
            tsetn(total, "planet", tnum(total, "planet") + 1);
            tsetn(total, "tarot_planet", tnum(total, "tarot_planet") + 1);
        } else if (streq(set, "Spectral")) {
            tsetn(total, "spectral", tnum(total, "spectral") + 1);
        }

        tsetn(total, "all", tnum(total, "all") + 1);

        if (!tbool(center, "discovered")) {
            // note: the Lua passes the Card itself, not its center: discover_card then reads and writes discovered,
            // wip and alert ON THE CARD, which are fields of its dyn table
            discover_card(card->moveable.node.object.dyn);
        }

        if (streq(set, "Tarot") || streq(set, "Planet")) {
            // note: the events keep the card pointer, as the Lua's closure does; both run on the next passes of the
            // queue, long before the used card is removed
            E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "immediate", .func = ev_last_tarot_planet, .ctx = card}), NULL, false);
        }
    }
    Game_save_settings(G);
}

// misc_functions.lua:1232
void set_voucher_usage(Card *card) {
    Table *config = card->moveable.node.config;
    const char *center_key = tstr(config, "center_key");
    if (center_key && streq(tstr(card->ability, "set"), "Voucher")) {
        Table *voucher_usage = ttab(cur_profile(), "voucher_usage");
        if (ttab(voucher_usage, center_key)) {
            tsetn(ttab(voucher_usage, center_key), "count", tnum(ttab(voucher_usage, center_key), "count") + 1);
        } else {
            tsett(voucher_usage, center_key, TAB("count", N(1), "order", tget(ttab(config, "center"), "order")));
        }
    }
    Game_save_settings(G);
}

// misc_functions.lua:1243
void set_hand_usage(const char *hand) {
    const char *hand_label = hand;
    // hand:gsub("%s+", "")
    char buf[256];
    int o = 0;
    for (const char *p = hand_label; *p && o < 255; p++) {
        if (!(*p == ' ' || (*p >= '\t' && *p <= '\r'))) buf[o++] = *p;
    }
    buf[o] = 0;
    hand = str(buf);
    Table *p_usage = ttab(cur_profile(), "hand_usage");
    if (ttab(p_usage, hand)) {
        tsetn(ttab(p_usage, hand), "count", tnum(ttab(p_usage, hand), "count") + 1);
    } else {
        tsett(p_usage, hand, TAB("count", N(1), "order", S(hand_label)));
    }
    Table *g_usage = ttab(G->GAME, "hand_usage");
    if (ttab(g_usage, hand)) {
        tsetn(ttab(g_usage, hand), "count", tnum(ttab(g_usage, hand), "count") + 1);
    } else {
        tsett(g_usage, hand, TAB("count", N(1), "order", S(hand_label)));
    }
    Game_save_settings(G);
}
