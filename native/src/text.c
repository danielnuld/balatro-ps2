// engine/text.lua:1-315: DynaText, the animated text.
#include "all.h"

// text.lua:2 DynaText = Moveable:extend()
Class DynaText_class = {
    .name = "DynaText",
    .super = &Moveable_class,
    .update = (void (*)(void *, float))DynaText_update,
    .draw = (void (*)(void *))DynaText_draw,
};

#define CONFIG(self) ((self)->moveable.node.config)
#define DYN(self) ((self)->moveable.node.object.dyn)

// config.key or def
static float cfg_or(Table *config, const char *key, float def) {
    Val v = tget(config, key);
    return truthy(v) ? (float)vnum(v) : def;
}

// text.lua:5
void DynaText_init(DynaText *self, Table *config) {
    if (!config) config = tnew();
    CONFIG(self) = config;
    // the copies of the config keys nothing assigns after creation (ui.h)
    self->cfg.spacing = cfg_or(config, "spacing", 0);
    self->cfg.min_cycle_time = cfg_or(config, "min_cycle_time", 1);
    self->cfg.shadow_colour = UI_colour(config, "shadow_colour");
    self->cfg.reset_pop_in = tbool(config, "reset_pop_in");
    self->cfg.non_recalc = tbool(config, "non_recalc");
    self->cfg.offset_y = cfg_or(config, "offset_y", 0);
    self->cfg.random_element = tbool(config, "random_element");
    self->cfg.pitch_shift = cfg_or(config, "pitch_shift", 0);
    self->cfg.pop_delay = cfg_or(config, "pop_delay", 1.5f);
    Val rotate = tget(config, "rotate");
    self->cfg.rotate = !truthy(rotate) ? 0 : (rotate.t == V_NUM && rotate.n == 2) ? 2 : 1;
    self->cfg.float_ = tbool(config, "float");
    self->cfg.bump = tbool(config, "bump");
    self->cfg.has_maxw = truthy(tget(config, "maxw"));
    self->cfg.maxw = cfg_or(config, "maxw", 0);

    self->shadow = tbool(config, "shadow");
    self->scale = cfg_or(config, "scale", 1);
    self->pop_in_rate = cfg_or(config, "pop_in_rate", 3);
    self->bump_rate = cfg_or(config, "bump_rate", 2.666f);
    self->bump_amount = cfg_or(config, "bump_amount", 1);
    self->font = ttab(config, "font");
    if (!self->font) self->font = ttab(G->LANG, "font");
    Val string = tget(config, "string");
    if (truthy(string) && string.t != V_TAB) tset(config, "string", T(ARR(string)));
    // note: text.lua:15 self.string = config.string[1] or {'HELLO WORLD'} is never read before align_letters
    // replaces it: NULL (ui.h)
    self->string = NULL;
    Table *TEXT_OFFSET = ttab(self->font, "TEXT_OFFSET");
    self->text_offset.x = tnum(TEXT_OFFSET, "x") * self->scale + cfg_or(config, "x_offset", 0);
    self->text_offset.y = tnum(TEXT_OFFSET, "y") * self->scale + cfg_or(config, "y_offset", 0);
    self->colours = ttab(config, "colours");
    if (!self->colours) self->colours = ARR(P(G->C.RED));
    self->created_time = G->TIMERS.REAL;
    self->silent = tbool(config, "silent");

    self->start_pop_in = tbool(config, "pop_in");

    tsetn(config, "W", 0);
    tsetn(config, "H", 0);

    self->strings = NULL;
    self->nstrings = 0;
    self->focused_string = 1;

    DynaText_update_text(self, true);
    if (self->cfg.has_maxw && tnum(config, "W") > self->cfg.maxw) {
        self->start_pop_in = tbool(config, "pop_in");
        self->scale = self->scale * (self->cfg.maxw / (float)tnum(config, "W"));
        DynaText_update_text(self, true);
    }

    if (self->nstrings > 1) {
        self->pop_delay = self->cfg.pop_delay;
        DynaText_pop_out(self, 4);
    }

    Moveable_init(&self->moveable, cfg_or(config, "X", 0), cfg_or(config, "Y", 0), (float)tnum(config, "W"),
                  (float)tnum(config, "H"));

    self->moveable.node.T->r = cfg_or(config, "text_rot", 0);

    self->moveable.node.states.hover.can = false;
    self->moveable.node.states.click.can = false;
    self->moveable.node.states.collide.can = false;
    self->moveable.node.states.drag.can = false;
    self->moveable.node.states.release_on.can = false;

    Moveable_set_role(&self->moveable, TAB("wh_bond", S("Weak"), "scale_bond", S("Weak")));

    if (self->moveable.node.object.cls == &DynaText_class) { // getmetatable(self) == DynaText
        tinsert(G->I.MOVEABLE, O(self));
    }
}

// text.lua:64
void DynaText_update(DynaText *self, float dt) {
    DynaText_update_text(self, false);
    DynaText_align_letters(self);
}

// The same value, bit for bit (not veq: 0 and -0 are equal and print differently; a NaN is the same NaN)
static bool same_val(Val a, Val b) {
    if (a.t != b.t) return false;
    if (a.t == V_NIL) return true;
    if (a.t == V_BOOL) return a.b == b.b;
    if (a.t == V_NUM) return memcmp(&a.n, &b.n, sizeof a.n) == 0;
    return a.p == b.p; // a string is interned; a table or an object prints its address
}

// DynaText.cfg_snap: config.string / pop_out / pop_in / pulse / quiver, read again only after a write to config
static const __typeof__(((DynaText *)0)->cfg_snap) *cfg_snap(DynaText *self) {
    Table *config = CONFIG(self);
    if (self->cfg_snap.config != config || self->cfg_snap.stamp != tstamp(config)) {
        self->cfg_snap.string = ttab(config, "string");
        self->cfg_snap.pop_out = tget(config, "pop_out"), self->cfg_snap.pop_in = tget(config, "pop_in");
        self->cfg_snap.pulse = ttab(config, "pulse"), self->cfg_snap.quiver = ttab(config, "quiver");
        self->cfg_snap.config = config, self->cfg_snap.stamp = tstamp(config);
    }
    return &self->cfg_snap;
}

// text.lua:69
void DynaText_update_text(DynaText *self, bool first_pass) {
    Table *config = CONFIG(self);
    // config.W and config.H: the Lua zeroes them, raises them string by string and reads them back; nothing else
    // looks at them in between, so they are locals here and written once after the loop (and not at all when
    // config still holds these values: wh_stamp)
    num cW = 0, cH = 0;

    Table *config_string = cfg_snap(self)->string;
    if (!config_string) ERROR("text.lua:73: bad argument #1 to 'ipairs' (table expected, got nil)");
    for (int k = 1;; k++) {
        Val v = tgeti(config_string, k);
        if (v.t == V_NIL) break;
        bool is_table = v.t == V_TAB;
        // A bound part whose own table and whose bound table were not written since this string was last left up
        // to date composes the same text again: nothing to do (ui.h, DynaString.idle_*)
        DynaString *idle = (is_table && !first_pass && k <= self->nstrings) ? &self->strings[k - 1] : NULL;
        if (idle && idle->idle_part == v.tab && idle->idle_ref && idle->idle_part_stamp == tstamp(v.tab) &&
            idle->idle_ref_stamp == tstamp(idle->idle_ref)) {
            // still up to date
        } else if ((is_table && truthy(tget(v.tab, "ref_table"))) || first_pass) {
            int part_a = 0, part_b = 1000000;
            Val new_string = v;
            const float *outer_colour = NULL;
            const float *inner_colour = NULL;
            float part_scale = 1;
            if (is_table && (truthy(tget(v.tab, "ref_table")) || truthy(tget(v.tab, "string")))) {
                Table *part = v.tab;
                Val pre = tget(part, "prefix"), suf = tget(part, "suffix");
                const char *prefix = truthy(pre) ? vstr(pre) : "";
                const char *suffix = truthy(suf) ? vstr(suf) : "";
                Val ref_table = tget(part, "ref_table");
                Val value = NIL;
                if (truthy(ref_table)) value = ui_ref_get(ref_table, tget(part, "ref_value"));
                if (!truthy(value)) value = tget(part, "string");
                // prefix..tostring(value)..suffix is built every frame for every bound part, and formatting a number
                // is thousands of library calls on the PS2. The last one is kept with what it was made of (the
                // same value bit for bit, the same interned prefix and suffix) and given again while those hold.
                DynaString *memo = k <= self->nstrings ? &self->strings[k - 1] : NULL;
                if (memo && memo->memo_string && same_val(memo->memo_value, value) && memo->memo_prefix == prefix && memo->memo_suffix == suffix) {
                    new_string.t = V_STR, new_string.s = memo->memo_string;
                } else {
                    new_string = S(fmt("%s%s%s", prefix, tostring(value), suffix));
                    if (memo) memo->memo_value = value, memo->memo_prefix = prefix, memo->memo_suffix = suffix, memo->memo_string = new_string.s;
                }
                part_a = str_len(prefix);
                part_b = str_len(new_string.s) - str_len(suffix);
                if (truthy(tget(part, "scale"))) part_scale = (float)tnum(part, "scale");
                if (first_pass) {
                    outer_colour = UI_colour(part, "outer_colour");
                    inner_colour = UI_colour(part, "colour");
                }
                v = new_string;
            }

            if (k > self->nstrings) { // self.strings[k] = self.strings[k] or {}
                self->strings = realloc(self->strings, sizeof(DynaString) * k);
                memset(&self->strings[self->nstrings], 0, sizeof(DynaString) * (k - self->nstrings));
                self->nstrings = k;
            }
            DynaString *sk = &self->strings[k - 1];
            const char *old_string = sk->string;
            // note: a number is walked by utf8.chars as its string; a table with neither ref_table nor string
            // makes the Lua fail there
            const char *vs = vstr(new_string);
            if (!streq(old_string, vs) || first_pass) {
                if (self->start_pop_in) self->reset_pop_in = true;
                self->reset_pop_in = self->reset_pop_in || self->cfg.reset_pop_in;
                if (!self->reset_pop_in) {
                    tset(config, "pop_out", NIL);
                    tset(config, "pop_in", NIL);
                } else {
                    if (!truthy(tget(config, "pop_in"))) tsetn(config, "pop_in", 0);
                    self->created_time = G->TIMERS.REAL;
                }
                if (!vs) ERROR("text.lua:111: bad argument #1 to 'gmatch' (string expected, got table)");
                sk->string = str(vs);
                DynaLetter *old_letters = sk->letters;
                int nold = sk->nletters;
                num tempW = 0;
                num tempH = 0;
                int current_letter = 1;
                // note: one slot per byte, which is at least the number of characters
                sk->letters = calloc(str_len(vs) + 1, sizeof(DynaLetter));

                Font *FONT = font_FONT(self->font);
                num FONTSCALE = tnum(self->font, "FONTSCALE");
                num TEXT_HEIGHT_SCALE = tnum(self->font, "TEXT_HEIGHT_SCALE");
                bool config_pop_in = truthy(tget(config, "pop_in"));
                for (Utf8Iter it = utf8_chars(vs, false); utf8_next(&it);) {
                    const char *c = it.c;
                    DynaLetter *old_letter = (old_letters && current_letter <= nold) ? &old_letters[current_letter - 1] : NULL;
                    DynaLetter *let_tab = &sk->letters[current_letter - 1];
                    let_tab->letter = love_graphics_newText(FONT, c);
                    let_tab->chr = c;
                    let_tab->scale = old_letter ? old_letter->scale : part_scale;
                    num tx = (num)Font_getWidth(FONT, c) * self->scale * part_scale * G->TILESCALE * FONTSCALE +
                             2.7 * self->cfg.spacing * G->TILESCALE * FONTSCALE;
                    num ty = (num)Font_getHeight(FONT) * self->scale * part_scale * G->TILESCALE * FONTSCALE * TEXT_HEIGHT_SCALE;
                    let_tab->offset = old_letter ? old_letter->offset : (Vec2){0, 0};
                    let_tab->dims = (Vec2){(float)(tx / (FONTSCALE * G->TILESCALE)), (float)(ty / (FONTSCALE * G->TILESCALE))};
                    let_tab->pop_in = first_pass ? (old_letter ? old_letter->pop_in : (config_pop_in ? 0 : 1)) : 1;
                    let_tab->prefix = current_letter <= part_a ? outer_colour : NULL;
                    let_tab->suffix = current_letter > part_b ? outer_colour : NULL;
                    let_tab->colour = inner_colour;
                    if (k > 1) let_tab->pop_in = 0;
                    tempW = tempW + tx / ((num)G->TILESIZE * G->TILESCALE);
                    tempH = lmax(ty / ((num)G->TILESIZE * G->TILESCALE), tempH);
                    current_letter = current_letter + 1;
                }
                sk->nletters = current_letter - 1;
                for (int i = 0; i < nold; i++) Text_release(old_letters[i].letter); // the Lua's collector did
                free(old_letters);

                sk->W = (float)tempW;
                sk->H = (float)tempH;
            }
        }

        if (k > self->nstrings) ERROR("text.lua:134: attempt to index field '?' (a nil value)");
        DynaString *sk = &self->strings[k - 1];
        // (config_string[k] again: the block above leaves the composed string in v)
        Table *part_tab = is_table ? tgeti(config_string, k).tab : NULL;
        if (is_table && !first_pass && !(idle && sk->idle_ref && sk->idle_part == part_tab && sk->idle_part_stamp == tstamp(part_tab) &&
                                         sk->idle_ref_stamp == tstamp(sk->idle_ref))) { // (after the block above: up to date)
            Val rt = tget(part_tab, "ref_table");
            bool keyed = rt.t == V_TAB && tget(part_tab, "ref_value").t == V_STR;
            sk->idle_part = part_tab, sk->idle_ref = keyed ? rt.tab : NULL;
            sk->idle_part_stamp = tstamp(part_tab), sk->idle_ref_stamp = tstamp(sk->idle_ref);
        }
        if (sk->W > cW) { cW = sk->W; sk->W_offset = 0; }
        if (sk->H > cH) { cH = sk->H; sk->H_offset = 0; }
    }
    if (!(self->wh_stamp == tstamp(config) && self->wh_W == cW && self->wh_H == cH)) {
        tsetn(config, "W", cW);
        tsetn(config, "H", cH);
        self->wh_stamp = tstamp(config), self->wh_W = cW, self->wh_H = cH;
    }

    // note: W and H go through float (DynaString.W/H), so config.W and T.w compare equal when nothing changed
    float W = (float)cW, H = (float)cH;
    Transform *T_ = self->moveable.node.T;
    if (T_) { // nil during init
        if ((T_->w != W || T_->h != H) && (!first_pass || self->reset_pop_in)) {
            tset(DYN(self), "ui_object_updated", B(true));
            tset(DYN(self), "non_recalc", tget(config, "non_recalc"));
        }
        T_->w = W;
        T_->h = H;
    }

    self->reset_pop_in = false;
    self->start_pop_in = false;

    for (int k = 0; k < self->nstrings; k++) {
        DynaString *v = &self->strings[k];
        v->W_offset = 0.5f * (W - v->W);
        v->H_offset = 0.5f * (H - v->H + self->cfg.offset_y);
    }
}

// text.lua:156
void DynaText_pop_out(DynaText *self, float pop_out_timer) {
    tsetn(CONFIG(self), "pop_out", pop_out_timer < 0 ? 1 : pop_out_timer);
    self->pop_out_time = (num)G->TIMERS.REAL + self->pop_delay;
}

// text.lua:161
void DynaText_pop_in(DynaText *self, float pop_in_timer) {
    self->reset_pop_in = true;
    tset(CONFIG(self), "pop_out", NIL);
    tsetn(CONFIG(self), "pop_in", pop_in_timer < 0 ? 0 : pop_in_timer);
    self->created_time = G->TIMERS.REAL;

    DynaString *fs = DynaText_focused(self);
    for (int k = 0; k < fs->nletters; k++) fs->letters[k].pop_in = 0;

    DynaText_update_text(self, false);
}

// text.lua:174
void DynaText_align_letters(DynaText *self) {
    Table *config = CONFIG(self);
    num REAL = G->TIMERS.REAL;
    if (self->pop_cycle) {
        self->focused_string = self->cfg.random_element ? (int)math_random_range(1, self->nstrings)
                               : self->focused_string == self->nstrings ? 1 : self->focused_string + 1;
        self->pop_cycle = false;
        DynaString *fs = DynaText_focused(self);
        for (int k = 0; k < fs->nletters; k++) fs->letters[k].pop_in = 0;
        tsetn(config, "pop_in", 0.1);
        tset(config, "pop_out", NIL);
        self->created_time = REAL;
    }
    DynaString *fs = DynaText_focused(self);
    self->string = fs->string;
    int n = fs->nletters;
    num rm = SETTINGS_snap()->reduced_motion ? 0 : 1;
    num mct = self->cfg.min_cycle_time; // config.min_cycle_time or 1
    // Read once, not once per letter: config.pop_out and config.pop_in are assigned below only in the last letter's
    // turn (k == n), config.quiver never, config.pulse where the local is cleared with it.
    Val pop_out = cfg_snap(self)->pop_out, pop_in = cfg_snap(self)->pop_in;
    Table *pulse = cfg_snap(self)->pulse, *quiver = cfg_snap(self)->quiver;
    // The letters' animation is float, as every animation of the port (docs/conventions.md): the PS2 has no double FPU.
    // The pop-in / pop-out timing and the pulse (transients, with thresholds) keep the Lua's numbers.
    float rmf = (float)rm, REALf = (float)G->TIMERS.REAL, FONTSCALEf = font_nums(self->font)->FONTSCALE;
    float sqrt_scale = sqrtf(self->scale);
    for (int k = 1; k <= n; k++) {
        DynaLetter *letter = &fs->letters[k - 1];
        if (truthy(pop_out)) {
            letter->pop_in = (float)lmin(1, lmax(mct - (REAL - self->pop_out_time) * vnum(pop_out) / mct, 0));
            letter->pop_in = letter->pop_in * letter->pop_in;
            if (k == n && letter->pop_in <= 0 && self->nstrings > 1) self->pop_cycle = true;
        } else if (truthy(pop_in)) {
            float prev_pop_in = letter->pop_in;
            int len = str_len(self->string); // #self.string: bytes
            letter->pop_in = (float)lmin(1, lmax((REAL - vnum(pop_in) - self->created_time) * len * self->pop_in_rate - k + 1,
                                                self->cfg.min_cycle_time == 0 ? 1 : 0));
            letter->pop_in = letter->pop_in * letter->pop_in;
            if (prev_pop_in <= 0 && letter->pop_in > 0 && !self->silent && (len < 10 || k % 2 == 0)) {
                Transform *T_ = self->moveable.node.T;
                if (T_->x > G->ROOM->T->w + 2 || T_->y > G->ROOM->T->h + 2 || T_->x < -2 || T_->y < -2) {
                } else {
                    play_sound("paper1", (float)(0.45 + 0.05 * math_random() + (0.3 / len) * k + self->cfg.pitch_shift), -1);
                }
            }
            if (k == n && letter->pop_in >= 1) {
                if (self->nstrings > 1) {
                    self->pop_delay = (float)(REAL - vnum(pop_in) - self->created_time + self->cfg.pop_delay);
                    DynaText_pop_out(self, 4);
                } else {
                    tset(config, "pop_in", NIL);
                }
            }
        }
        float r = 0;
        float scale = 1;
        if (self->cfg.rotate)
            r = (self->cfg.rotate == 2 ? -1 : 1) * (0.2f * (-n / 2.0f - 0.5f + k) / n + rmf * 0.02f * sinf(wrap_angle(2 * REALf + k)));
        if (pulse) {
            num width = tnum(pulse, "width"), start = tnum(pulse, "start"), speed = tnum(pulse, "speed");
            scale = (float)(scale + rm * (1 / width) * tnum(pulse, "amount") *
                                        (lmax(lmin((start - REAL) * speed + k + width, (REAL - start) * speed - k + width + 2), 0)));
            r = (float)(r + rm * ((num)scale - 1) * (0.02 * (-n / 2.0 - 0.5 + k)));
            if (start > REAL + 2 * speed * n) {
                tset(config, "pulse", NIL);
                pulse = NULL;
            }
        }
        if (quiver) {
            float amount = (float)tnum(quiver, "amount"), speed = (float)tnum(quiver, "speed");
            scale = scale + rmf * (0.1f * amount);
            r = r + rmf * 0.3f * amount *
                        (sinf(wrap_angle(41.12342f * REALf * speed + k * 1223.2f)) +
                         cosf(wrap_angle(63.21231f * REALf * speed + k * 1112.2f)) * sinf(wrap_angle(36.1231f * REALf * speed)) +
                         cosf(wrap_angle(95.123f * REALf * speed + k * 1233.2f)) -
                         sinf(wrap_angle(30.133421f * REALf * speed + k * 123.2f)));
        }
        letter->r = r;
        letter->scale = scale;
        if (self->cfg.float_)
            letter->offset.y = rmf * sqrt_scale * (2 + (FONTSCALEf / G->TILESIZE) * 2000 * sinf(wrap_angle(2.666f * REALf + 200 * k))) +
                               60 * (scale - 1);
        if (self->cfg.bump)
            letter->offset.y = rmf * self->bump_amount * sqrt_scale * 7 *
                               fmaxf(0, (5 + self->bump_rate) * sinf(wrap_angle(self->bump_rate * REALf + 200 * k)) - 3 - self->bump_rate);
    }
}

// text.lua:239
void DynaText_set_quiver(DynaText *self, float amt) {
    tsett(CONFIG(self), "quiver", TAB("speed", N(0.5), "amount", N(amt < 0 ? 0.7 : amt), "silent", B(false)));
}

// text.lua:247
void DynaText_pulse(DynaText *self, float amt) {
    tsett(CONFIG(self), "pulse",
          TAB("speed", N(40), "width", N(2.5), "start", N(G->TIMERS.REAL), "amount", N(amt < 0 ? 0.2 : amt), "silent", B(false)));
}

// text.lua:257
void DynaText_draw(DynaText *self) {
    Val particle_effect = tget(self->moveable.node.children, "particle_effect");
    if (truthy(particle_effect)) CALL(particle_effect.obj, draw);

    DynaString *fs = DynaText_focused(self);
    float u = font_nums(self->font)->FONTSCALE / G->TILESIZE; // self.font.FONTSCALE/G.TILESIZE
    Vec2 sp = self->moveable.shadow_parrallax;
    bool always_shown = self->cfg.min_cycle_time == 0;

    if (self->shadow) {
        prep_draw(self, 1, 0, NULL);
        love_graphics_translate(fs->W_offset + self->text_offset.x * u, fs->H_offset + self->text_offset.y * u);
        if (self->cfg.spacing != 0) love_graphics_translate(self->cfg.spacing * u, 0); // 0 as nil: the same picture
        if (self->cfg.shadow_colour) {
            love_graphics_setColorv(self->cfg.shadow_colour);
        } else {
            love_graphics_setColor(0, 0, 0, 0.3f * ((const float *)tgeti(self->colours, 1).p)[3]);
        }
        for (int k = 1; k <= fs->nletters; k++) {
            DynaLetter *letter = &fs->letters[k - 1];
            float real_pop_in = always_shown ? 1 : letter->pop_in;
            love_graphics_draw_text(
                letter->letter,
                0.5f * (letter->dims.x - letter->offset.x) * u - sp.x * self->scale / (G->TILESIZE),
                0.5f * (letter->dims.y) * u - sp.y * self->scale / (G->TILESIZE),
                letter->r,
                real_pop_in * self->scale * u,
                real_pop_in * self->scale * u,
                0.5f * letter->dims.x / self->scale,
                0.5f * letter->dims.y / self->scale);
            love_graphics_translate(letter->dims.x * u, 0);
        }
        love_graphics_pop();
    }

    prep_draw(self, 1, 0, NULL);
    love_graphics_translate(fs->W_offset + self->text_offset.x * u, fs->H_offset + self->text_offset.y * u);
    if (self->cfg.spacing != 0) love_graphics_translate(self->cfg.spacing * u, 0);
    // note: NaN when shadow_parrallax is (0, 0), as in the Lua (Moveable keeps y = -1.5, so it does not happen)
    Vec2 _shadow_norm = {
        sp.x / sqrtf(sp.y * sp.y + sp.x * sp.x) * u,
        sp.y / sqrtf(sp.y * sp.y + sp.x * sp.x) * u,
    };

    int ncolours = tlen(self->colours);
    for (int k = 1; k <= fs->nletters; k++) {
        DynaLetter *letter = &fs->letters[k - 1];
        float real_pop_in = always_shown ? 1 : letter->pop_in;
        const float *colour = letter->prefix ? letter->prefix : letter->suffix ? letter->suffix : letter->colour ? letter->colour
                              : (const float *)tgeti(self->colours, k % ncolours + 1).p;
        love_graphics_setColorv(colour);

        love_graphics_draw_text(
            letter->letter,
            0.5f * (letter->dims.x - letter->offset.x) * u + _shadow_norm.x,
            0.5f * (letter->dims.y - letter->offset.y) * u + _shadow_norm.y,
            letter->r,
            real_pop_in * letter->scale * self->scale * u,
            real_pop_in * letter->scale * self->scale * u,
            0.5f * letter->dims.x / (self->scale),
            0.5f * letter->dims.y / (self->scale));
        love_graphics_translate(letter->dims.x * u, 0);
    }
    love_graphics_pop();

    add_to_drawhash(self);
    Node_draw_boundingrect((Node *)self);
}
