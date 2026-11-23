// engine/ui.lua:631-1054: the per-frame half of UIElement (update_object, draw_self, draw_pixellated_rect, update)
// and its input callbacks (click, hover, release, remove...).
#include "all.h"

#define CFG(self) ((self)->moveable.node.config)
#define SHADOWS_ON() (SETTINGS_snap()->shadows_on)

// ui.h UI_snap: reads the keys again
void UI_snap_fill(UIElement *e) {
    Table *config = CFG(e);
    struct UISnap *s = &e->snap;
    s->button = tget(config, "button"), s->button_temp = tget(config, "button_temp");
    s->button_delay = tget(config, "button_delay"), s->func = tget(config, "func");
    s->scale = tget(config, "scale"), s->outline = tget(config, "outline"), s->chosen = tget(config, "chosen");
    s->text = tget(config, "text"), s->ref_table = tget(config, "ref_table"), s->prev_value = tget(config, "prev_value");
    s->object = tget(config, "object"), s->text_drawable = tget(config, "text_drawable");
    s->button_UIE = tobj(config, "button_UIE");
    s->colour = UI_colour(config, "colour"), s->outline_colour = UI_colour(config, "outline_colour");
    s->lang = ttab(config, "lang");
    s->shadow = tbool(config, "shadow"), s->hover = tbool(config, "hover");
    s->force_focus = tbool(config, "force_focus"), s->draw_after = tbool(config, "draw_after");
    s->config = config, s->stamp = tstamp(config); // (last: the reads above write nothing)
}

// ui.h font_nums, lang_font. In statics: the collector sees the tables there, so the addresses are not reused.
const struct FontNums *font_nums(Table *font) {
    static struct FontNums f;
    if (!font || font != f.font || f.stamp != tstamp(font) || f.ostamp != tstamp(f.offs)) {
        f.font = font, f.offs = ttab(font, "TEXT_OFFSET");
        f.FONTSCALE = (float)tnum(font, "FONTSCALE"), f.squish = (float)tnum(font, "squish");
        f.off_x = (float)tnum(f.offs, "x"), f.off_y = (float)tnum(f.offs, "y");
        f.stamp = tstamp(font), f.ostamp = tstamp(f.offs);
    }
    return &f;
}
Table *lang_font(Table *lang) {
    static Table *seen, *font;
    static uint64_t stamp;
    if (!lang || lang != seen || stamp != tstamp(lang)) seen = lang, font = ttab(lang, "font"), stamp = tstamp(lang);
    return font;
}

// tobj() of a value already read
static inline void *val_obj(Val v) { return v.t == V_OBJ || v.t == V_PTR ? v.p : NULL; }
// the same value, bit for bit (writing it again would change nothing)
static inline bool val_same(Val a, Val b) {
    return a.t == b.t && (a.t == V_NIL || (a.t == V_NUM ? memcmp(&a.n, &b.n, sizeof a.n) == 0 : a.t == V_BOOL ? a.b == b.b : a.p == b.p));
}

// ui.lua:631
void UIElement_update_object(UIElement *self) {
    Table *config = CFG(self);
    Val ref_table = UI_snap(self)->ref_table;
    if (truthy(ref_table) && truthy(self->cfg.ref_value)) {
        Val v = ui_ref_get(ref_table, self->cfg.ref_value);
        if (!veq(v, UI_snap(self)->object)) {
            tset(config, "object", v);
            UIBox_recalculate(self->UIBox);
        }
    }

    Moveable *object = val_obj(UI_snap(self)->object);
    if (object) {
        // (not written again when it is `true` already: the write would change nothing)
        if (!(Moveable_refresh_movement(object) && object->cfg_snap.is_true)) tset(object->node.config, "refresh_movement", B(true));
        if (object->node.states.hover.is && !self->moveable.node.states.hover.is) {
            UIElement_hover(self);
            self->moveable.node.states.hover.is = true;
        }
        if (!object->node.states.hover.is && self->moveable.node.states.hover.is) {
            UIElement_stop_hover(self);
            self->moveable.node.states.hover.is = false;
        }
    }

    // note: hover / stop_hover do not assign config.object, so `object` is still current here
    // (object.ui_object_updated is read again only after the object's dyn was written: idle_dyn, ui.h)
    Table *odyn = object ? object->node.object.dyn : NULL;
    bool updated = false;
    if (object && !(self->idle_dyn == odyn && self->idle_dyn_stamp == tstamp(odyn))) {
        updated = tbool(odyn, "ui_object_updated");
        if (!updated) self->idle_dyn = odyn, self->idle_dyn_stamp = tstamp(odyn);
    }
    if (updated) {
        tset(object->node.object.dyn, "ui_object_updated", NIL);
        object->node.parent = (Node *)self;
        // note: no class overrides set_role / move_with_major: Moveable's are called by name
        Moveable_set_role(object, self->cfg.role ? self->cfg.role : TAB("role_type", S("Minor"), "major", O(self)));
        Moveable_move_with_major(object, 0);
        if (tbool(object->node.object.dyn, "non_recalc")) {
            UIElement *parent = (UIElement *)self->moveable.node.parent;
            parent->content_dimensions.w = object->node.T->w;
            UIElement_align(self, parent->moveable.node.T->x - object->node.T->x, parent->moveable.node.T->y - object->node.T->y);
            UIElement_set_alignments(parent);
        } else {
            UIBox_recalculate(self->UIBox);
        }
    }
}

// ui.lua:781-790 and :820-829: the outline of a focused element / object. timer: the focus timer's value.
static void draw_focus_outline(UIElement *self, num timer, float parallax_dist) {
    float tmp[4];
    const float *colour = UI_snap(self)->colour;
    float lw = 50 * powf((float)lmax(0, timer - G->TIMERS.REAL + 0.3), 2);
    prep_draw(self, 1, 0, NULL);
    love_graphics_scale(1 / G->TILESIZE, 1 / G->TILESIZE);
    love_graphics_setLineWidth(lw + 1.5f);
    love_graphics_setColorv(adjust_alpha(G->C.WHITE, 0.2f * lw, tmp));
    UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL, NIL);
    love_graphics_setColorv(colour[3] > 0 ? mix_colours(G->C.WHITE, colour, 0.8f, tmp) : G->C.WHITE);
    UIElement_draw_pixellated_rect(self, "line", parallax_dist, NIL, NIL);
    love_graphics_pop();
}

// ui.lua:663
void UIElement_draw_self(UIElement *self) {
    Table *config = CFG(self);
    Node *node = &self->moveable.node;
    Transform *VT = &self->moveable.VT;
    Vec2 *sp = &self->moveable.shadow_parrallax;
    float TS = G->TILESIZE;
    float tmp[4];

    // The config keys come from the element's snapshot (ui.h, UI_snap): read from the table again only after a
    // write to it. Nothing below writes config before the object's draw, after which the snapshot is asked again.
    const struct UISnap *s = UI_snap(self);
    if (!node->states.visible) {
        if (s->force_focus) add_to_drawhash(self);
        return;
    }

    UIElement *button_UIE = s->button_UIE;
    bool button = truthy(s->button);
    if (s->force_focus || self->cfg.force_collision || button_UIE || button || node->states.collide.can) {
        add_to_drawhash(self);
    }

    bool button_active = true;
    float parallax_dist = 1.5f;
    bool button_being_pressed = false;
    bool shadow = s->shadow;

    if (button || button_UIE) {
        Moveable *par = (node->parent && node->parent != (Node *)self->UIBox) ? (Moveable *)node->parent : NULL;
        self->moveable.layered_parallax.x = (par ? par->layered_parallax.x : 0) + (shadow ? 0.4f * sp->x : 0) / TS;
        self->moveable.layered_parallax.y = (par ? par->layered_parallax.y : 0) + (shadow ? 0.4f * sp->y : 0) / TS;

        if (button && ((self->has_last_clicked && self->last_clicked > G->TIMERS.REAL - 0.1) ||
                       ((node->states.hover.is || node->states.drag.is) && G->CONTROLLER->is_cursor_down))) {
            self->moveable.layered_parallax.x -= parallax_dist * sp->x / TS * self->cfg.button_dist;
            self->moveable.layered_parallax.y -= parallax_dist * sp->y / TS * self->cfg.button_dist;
            parallax_dist = 0;
            button_being_pressed = true;
        }

        if (button_UIE && !truthy(UI_snap(button_UIE)->button)) button_active = false;
    }

    const float *colour = s->colour;
    Moveable *object = NULL;
    if (colour[3] > 0.01f) {
        if (self->UIT == UIT_T && truthy(s->scale)) {
            float scale = (float)vnum(s->scale);
            const struct FontNums *fn = font_nums(lang_font(s->lang));
            float FONTSCALE = fn->FONTSCALE, squish = fn->squish, off_x = fn->off_x, off_y = fn->off_y;
            Text *text_drawable = (Text *)s->text_drawable.p;
            bool vert = self->cfg.vert;
            float sx = -sp->x * 0.5f / (scale * FONTSCALE);
            float sy = -sp->y * 0.5f / (scale * FONTSCALE);

            if ((button_UIE && button_active) || (!button_UIE && shadow && SHADOWS_ON())) {
                prep_draw(self, 0.97f, 0, NULL);
                if (vert) { love_graphics_translate(0, VT->h); love_graphics_rotate(-(float)M_PI / 2); }
                if ((shadow || (button_UIE && button_active)) && SHADOWS_ON()) {
                    love_graphics_setColor(0, 0, 0, 0.3f * colour[3]);
                    love_graphics_draw_text(text_drawable,
                        (off_x + (vert ? -sy : sx)) * scale * FONTSCALE / TS,
                        (off_y + (vert ? sx : sy)) * scale * FONTSCALE / TS,
                        0,
                        scale * squish * FONTSCALE / TS,
                        scale * FONTSCALE / TS, 0, 0);
                }
                love_graphics_pop();
            }

            prep_draw(self, 1, 0, NULL);
            if (vert) { love_graphics_translate(0, VT->h); love_graphics_rotate(-(float)M_PI / 2); }
            if (!button_active) {
                love_graphics_setColorv(G->C.UI.TEXT_INACTIVE);
            } else {
                love_graphics_setColorv(colour);
            }
            love_graphics_draw_text(text_drawable,
                off_x * scale * FONTSCALE / TS,
                off_y * scale * FONTSCALE / TS,
                0,
                scale * squish * FONTSCALE / TS,
                scale * FONTSCALE / TS, 0, 0);
            love_graphics_pop();
        } else if (self->UIT == UIT_B || self->UIT == UIT_C || self->UIT == UIT_R || self->UIT == UIT_ROOT) {
            prep_draw(self, 1, 0, NULL);
            love_graphics_scale(1 / TS, 1 / TS);
            if (shadow && SHADOWS_ON()) {
                love_graphics_scale(0.98f, 0.98f);
                if (self->cfg.shadow_colour) {
                    love_graphics_setColorv(self->cfg.shadow_colour);
                } else {
                    love_graphics_setColor(0, 0, 0, 0.3f * colour[3]);
                }
                if (self->cfg.r && VT->w > 0.01f) {
                    UIElement_draw_pixellated_rect(self, "shadow", parallax_dist, NIL, NIL);
                } else {
                    love_graphics_rectangle("fill", -sp->x * parallax_dist, -sp->y * parallax_dist, VT->w * TS, VT->h * TS);
                }
                love_graphics_scale(1 / 0.98f, 1 / 0.98f);
            }

            love_graphics_scale(button_being_pressed ? 0.985f : 1, button_being_pressed ? 0.985f : 1);
            if (self->cfg.has_emboss) {
                love_graphics_setColorv(darken(colour, node->states.hover.is ? 0.5f : 0.3f, tmp));
                UIElement_draw_pixellated_rect(self, "emboss", parallax_dist, N(self->cfg.emboss), NIL);
            }
            UIElement *collided_button = button_UIE ? button_UIE : self;
            bool button_delay = truthy(s->button_delay);
            float mixed[4];
            const float *button_colours[2];
            int n = 1;
            button_colours[0] = button_delay ? mix_colours(colour, G->C.L_BLACK, 0.5f, mixed) : colour;
            if ((UI_snap(collided_button)->hover && collided_button->moveable.node.states.hover.is) ||
                (collided_button->has_last_clicked && collided_button->last_clicked > G->TIMERS.REAL - 0.1)) {
                button_colours[n++] = G->C.UI.HOVER;
            }
            for (int k = 0; k < n; k++) {
                const float *v = button_colours[k];
                love_graphics_setColorv(v);
                if (self->cfg.r && VT->w > 0.01f) {
                    if (button_delay) {
                        love_graphics_setColorv(G->C.GREY);
                        UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL, NIL);
                        love_graphics_setColorv(v);
                        UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL, tget(config, "button_delay_progress"));
                    } else if (self->cfg.progress_bar) {
                        Table *pb = self->cfg.progress_bar;
                        const float *empty_col = UI_colour(pb, "empty_col"), *filled_col = UI_colour(pb, "filled_col");
                        love_graphics_setColorv(empty_col ? empty_col : G->C.GREY);
                        UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL, NIL);
                        love_graphics_setColorv(filled_col ? filled_col : G->C.BLUE);
                        UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL,
                            N(vnum(ui_ref_get(tget(pb, "ref_table"), tget(pb, "ref_value"))) / tnum(pb, "max")));
                    } else {
                        UIElement_draw_pixellated_rect(self, "fill", parallax_dist, NIL, NIL);
                    }
                } else {
                    love_graphics_rectangle("fill", 0, 0, VT->w * TS, VT->h * TS);
                }
            }
            love_graphics_pop();
        } else if (self->UIT == UIT_O && (object = val_obj(s->object))) {
            // Draw the outline for highlighted object
            if (self->cfg.focus_with_object && object->node.states.focus.is) {
                if (!self->has_object_focus_timer) { self->has_object_focus_timer = true; self->object_focus_timer = G->TIMERS.REAL; }
                draw_focus_outline(self, self->object_focus_timer, parallax_dist);
            } else {
                self->has_object_focus_timer = false;
            }
            CALL(object, draw);
            s = UI_snap(self); // (whatever the object's draw did)
        }
    }

    // Draw the outline of the object
    Val outline = s->outline;
    if (truthy(outline) && s->outline_colour[3] > 0.01f) {
        const float *outline_colour = s->outline_colour;
        prep_draw(self, 1, 0, NULL);
        love_graphics_scale(1 / TS, 1 / TS);
        love_graphics_setLineWidth((float)vnum(outline));
        if (self->cfg.has_line_emboss) {
            love_graphics_setColorv(darken(outline_colour, node->states.hover.is ? 0.5f : 0.3f, tmp));
            UIElement_draw_pixellated_rect(self, "line_emboss", parallax_dist, N(self->cfg.line_emboss), NIL);
        }
        love_graphics_setColorv(outline_colour);
        if (self->cfg.r && VT->w > 0.01f) {
            UIElement_draw_pixellated_rect(self, "line", parallax_dist, NIL, NIL);
        } else {
            love_graphics_rectangle("line", 0, 0, VT->w * TS, VT->h * TS);
        }
        love_graphics_pop();
    }

    // Draw the outline for highlighted buttons
    if (node->states.focus.is) {
        if (!self->has_focus_timer) { self->has_focus_timer = true; self->focus_timer = G->TIMERS.REAL; }
        draw_focus_outline(self, self->focus_timer, parallax_dist);
    } else {
        self->has_focus_timer = false;
    }

    // Draw the 'chosen triangle'
    Val chosen = s->chosen;
    if (truthy(chosen)) {
        bool vert = chosen.t == V_STR && streq(chosen.s, "vert");
        float tri[6];
        prep_draw(self, 0.98f, 0, NULL);
        love_graphics_scale(1 / TS, 1 / TS);
        if (shadow && SHADOWS_ON()) {
            love_graphics_setColor(0, 0, 0, 0.3f * colour[3]);
            get_chosen_triangle_from_rect(self->moveable.layered_parallax.x - sp->x * parallax_dist * 0.5f,
                                          self->moveable.layered_parallax.y - sp->y * parallax_dist * 0.5f, VT->w * TS, VT->h * TS, vert, tri);
            love_graphics_polygon("fill", tri, 3);
        }
        love_graphics_pop();

        prep_draw(self, 1, 0, NULL);
        love_graphics_scale(1 / TS, 1 / TS);
        love_graphics_setColorv(G->C.RED);
        get_chosen_triangle_from_rect(self->moveable.layered_parallax.x, self->moveable.layered_parallax.y, VT->w * TS, VT->h * TS, vert, tri);
        love_graphics_polygon("fill", tri, 3);
        love_graphics_pop();
    }
    Node_draw_boundingrect(node);
}

// ui.lua:853
void UIElement_draw_pixellated_rect(UIElement *self, const char *_type, float _parallax, Val _emboss, Val _progress) {
    Transform *VT = &self->moveable.VT;
    Vec2 *sp = &self->moveable.shadow_parrallax;
    UIPixellatedRect *pr = self->pixellated_rect;
    bool is_emboss = streq(_type, "emboss"), is_line_emboss = streq(_type, "line_emboss");
    float progress = truthy(_progress) ? (float)vnum(_progress) : 1;
    // note: #pixellated_rect[_type].vertices < 1 can only be true for the two emboss lists (built without _emboss)
    if (!pr ||
        ((is_emboss || is_line_emboss) && !pr->has_emboss) ||
        _parallax != pr->parallax ||
        pr->w != VT->w ||
        pr->h != VT->h ||
        pr->sw != sp->x ||
        pr->sh != sp->y ||
        pr->progress != progress) {
        if (!pr) pr = self->pixellated_rect = calloc(1, sizeof(UIPixellatedRect));
        pr->w = VT->w;
        pr->h = VT->h;
        pr->sw = sp->x;
        pr->sh = sp->y;
        pr->progress = progress;
        pr->parallax = _parallax;
        pr->has_emboss = truthy(_emboss);
        float emboss = (float)vnum(_emboss);
        float TS = G->TILESIZE;
        float ext_up = self->cfg.ext_up * TS; // (0 as nil: the same 0)
        float m = fminf(VT->w, VT->h + fabsf(ext_up) / TS);
        float res = self->cfg.has_res ? self->cfg.res : m > 3.5f ? 0.8f : m > 0.3f ? 0.6f : 0.15f;
        float totw = VT->w * TS, toth = (VT->h + fabsf(ext_up) / TS) * TS;
        float subw = VT->w * TS - 4 * res, subh = (VT->h + fabsf(ext_up) / TS) * TS - 4 * res;

        float vertices[60] = {
            subw / 2, subh / 2 - ext_up,
            0, 4 * res - ext_up,
            1 * res, 4 * res - ext_up,
            1 * res, 2 * res - ext_up,
            2 * res, 2 * res - ext_up,
            2 * res, 1 * res - ext_up,
            4 * res, 1 * res - ext_up,
            4 * res, 0 * res - ext_up,
            subw, 0 * res - ext_up,
            subw, 1 * res - ext_up,
            subw + 2 * res, 1 * res - ext_up,
            subw + 2 * res, 2 * res - ext_up,
            subw + 3 * res, 2 * res - ext_up,
            subw + 3 * res, 4 * res - ext_up,
            totw, 4 * res - ext_up,
            totw, subh - ext_up,
            subw + 3 * res, subh - ext_up,
            subw + 3 * res, subh + 2 * res - ext_up,
            subw + 2 * res, subh + 2 * res - ext_up,
            subw + 2 * res, subh + 3 * res - ext_up,
            subw, subh + 3 * res - ext_up,
            subw, toth - ext_up,
            4 * res, toth - ext_up,
            4 * res, subh + 3 * res - ext_up,
            2 * res, subh + 3 * res - ext_up,
            2 * res, subh + 2 * res - ext_up,
            1 * res, subh + 2 * res - ext_up,
            1 * res, subh - ext_up,
            0, subh - ext_up,
            0, 4 * res - ext_up,
        };
        for (int k = 1; k <= 60; k++) { // k: the Lua's 1-based index (odd: x, even: y)
            float v = vertices[k - 1];
            if (k % 2 == 1 && v > totw * pr->progress) v = totw * pr->progress;
            pr->fill[k - 1] = v;
            if (k > 4) {
                pr->line[k - 5] = v;
                if (pr->has_emboss) {
                    pr->line_emboss[k - 5] = v + (k % 2 == 0 ? -emboss * sp->y : -0.7f * emboss * sp->x);
                }
            }
            if (k % 2 == 0) {
                pr->shadow[k - 1] = v - sp->y * _parallax;
                if (pr->has_emboss) {
                    pr->emboss[k - 1] = v + emboss * TS;
                }
            } else {
                pr->shadow[k - 1] = v - sp->x * _parallax;
                if (pr->has_emboss) {
                    pr->emboss[k - 1] = v;
                }
            }
        }
    }

    // note: an emboss list asked for without an _emboss is empty in the Lua (LOVE would fail); the game never does it
    if (streq(_type, "line")) love_graphics_polygon("line", pr->line, 28);
    else if (is_line_emboss) love_graphics_polygon("line", pr->line_emboss, 28);
    else if (is_emboss) love_graphics_polygon("fill", pr->emboss, 30);
    else if (streq(_type, "shadow")) love_graphics_polygon("fill", pr->shadow, 30);
    else love_graphics_polygon("fill", pr->fill, 30);
}

// ui.lua:938
void UIElement_update(UIElement *self, float dt) {
    Table *config = CFG(self);
    // (G.ARGS.FUNC_TRACKER, :939 and :949: a debug counter nothing reads; not translated)
    Val button_delay = UI_snap(self)->button_delay;
    if (truthy(button_delay)) {
        Val button = tget(config, "button");
        tset(config, "button_temp", truthy(button) ? button : tget(config, "button_temp"));
        tset(config, "button", NIL);
        tsetn(config, "button_delay_progress", (G->TIMERS.REAL - tnum(config, "button_delay_start")) / vnum(button_delay));
        if (G->TIMERS.REAL >= tnum(config, "button_delay_end")) tset(config, "button_delay", NIL);
    }
    {
        // (config.button = config.button_temp is not written again when it holds that value already)
        const struct UISnap *s = UI_snap(self);
        if (truthy(s->button_temp) && !truthy(s->button_delay) && !val_same(s->button, s->button_temp)) tset(config, "button", s->button_temp);
    }
    if (self->button_clicked) self->button_clicked = false;
    Val func_v = UI_snap(self)->func;
    if (truthy(func_v)) {
        const char *func = func_v.t == V_STR ? func_v.s : NULL;
        FuncsFn fn = funcs_lookup(func);
        if (!fn) ERROR("ui.lua:950: G.FUNCS.%s is nil", func ? func : "?");
        fn(self);
    }
    if (self->UIT == UIT_T) UIElement_update_text(self);
    if (self->UIT == UIT_O) UIElement_update_object(self);
    Node_update((Node *)self, dt);
}

// ui.lua:957
bool UIElement_collides_with_point(UIElement *self, Vec2 cursor_trans) {
    if (self->UIBox->moveable.node.states.collide.can) {
        return Node_collides_with_point((Node *)self, cursor_trans);
    } else {
        return false;
    }
}

// ui.lua:965
void UIElement_click(UIElement *self) {
    Table *config = CFG(self);
    Val button = tget(config, "button");
    if (truthy(button) && (!self->has_last_clicked || self->last_clicked + 0.1 < G->TIMERS.REAL) && self->moveable.node.states.visible &&
        !self->moveable.node.under_overlay && !self->disable_button) {
        if (tbool(config, "one_press")) self->disable_button = true;
        self->has_last_clicked = true;
        self->last_clicked = G->TIMERS.REAL;

        // Removes a layer from the overlay menu stack
        Val id = tget(config, "id");
        if (id.t == V_STR && streq(id.s, "overlay_menu_back_button")) {
            Controller_mod_cursor_context_layer(G->CONTROLLER, -1);
            G->NO_MOD_CURSOR_STACK = true;
        }
        if (G->OVERLAY_TUTORIAL && veq(tget(G->OVERLAY_TUTORIAL->moveable.node.object.dyn, "button_listen"), button)) {
            FUNCS_tut_next(NULL);
        }
        FuncsFn fn = funcs_lookup(vstr(button));
        if (!fn) ERROR("ui.lua:978: G.FUNCS.%s is nil", vstr(button) ? vstr(button) : "?");
        fn(self);

        G->NO_MOD_CURSOR_STACK = false;

        // note: the callback may have removed this element; as in the Lua, the object is still there to finish on
        if (tbool(config, "choice")) {
            Table *choices = UIBox_get_group(self->UIBox, NULL, tget(config, "group"), NULL);
            for (TIter it = titer(choices); tnext(&it);) {
                UIElement *v = it.val.obj;
                if (tbool(CFG(v), "choice")) tset(CFG(v), "chosen", B(false));
            }
            tset(config, "chosen", B(true));
        }
        play_sound("button", 1, 0.3f);
        tsetn(G->ROOM->object.dyn, "jiggle", tnum(G->ROOM->object.dyn, "jiggle") + 0.5);
        self->button_clicked = true;
    }
    UIElement *button_UIE = tobj(config, "button_UIE");
    if (button_UIE) {
        UIElement_click(button_UIE);
    }
}

// ui.lua:998
void UIElement_put_focused_cursor(UIElement *self, float *x, float *y) {
    Table *focus_args = ttab(CFG(self), "focus_args");
    if (focus_args && streq(tstr(focus_args, "type"), "tab")) {
        // note: pairs(self.children); like the Lua, a child without children[1] (a keyed popup box) is not expected here
        for (TIter it = titer(self->moveable.node.children); tnext(&it);) {
            Node *v = it.val.obj;
            UIElement *first = tobji(v->children, 1);
            if (tbool(CFG(first), "chosen")) {
                UIElement_put_focused_cursor(first, x, y);
                return;
            }
        }
    } else {
        Node_put_focused_cursor((Node *)self, x, y);
    }
}

// ui.lua:1008
void UIElement_remove(UIElement *self) {
    Table *config = CFG(self);
    void *object = tobj(config, "object");
    if (object) {
        CALL(object, remove);
        tset(config, "object", NIL);
    }

    if (self == G->CONTROLLER->text_input_hook) {
        G->CONTROLLER->text_input_hook = NULL;
    }
    remove_all(self->moveable.node.children);

    // note: not in the Lua (its collector does it): ui.h has remove free the vertex cache
    free(self->pixellated_rect);
    self->pixellated_rect = NULL;

    Moveable_remove((Moveable *)self);
}

// ui.lua:1022
void UIElement_hover(UIElement *self) {
    Table *config = CFG(self);
    if (tbool(config, "on_demand_tooltip")) {
        bool below = self->moveable.node.T->y > G->ROOM->T->h / 2;
        tsett(config, "h_popup", create_popup_UIBox_tooltip(ttab(config, "on_demand_tooltip")));
        tsett(config, "h_popup_config", TAB("align", S(below ? "tm" : "bm"), "offset", T(TAB("x", N(0), "y", N(below ? -0.1 : 0.1))), "parent", O(self)));
    }
    if (tbool(config, "tooltip")) {
        tsett(config, "h_popup", create_popup_UIBox_tooltip(ttab(config, "tooltip")));
        tsett(config, "h_popup_config", TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.1))), "parent", O(self)));
    }
    if (tbool(config, "detailed_tooltip") && G->CONTROLLER->HID.pointer) {
        tsett(config, "h_popup", create_UIBox_detailed_tooltip(ttab(config, "detailed_tooltip")));
        tsett(config, "h_popup_config", TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.1))), "parent", O(self)));
    }
    Node_hover((Node *)self);
}

// ui.lua:1038
void UIElement_stop_hover(UIElement *self) {
    Node_stop_hover((Node *)self);
    if (tbool(CFG(self), "on_demand_tooltip")) {
        tset(CFG(self), "h_popup", NIL);
    }
}

// ui.lua:1045
void UIElement_release(UIElement *self, void *other) {
    if (self->moveable.node.parent) CALL(self->moveable.node.parent, release, other);
}

// ui.lua:1049
bool is_UI_containter(UIElement *node) {
    if (node->UIT != UIT_C && node->UIT != UIT_R && node->UIT != UIT_ROOT) {
        return false;
    }
    return true;
}
