// engine/ui.lua:1-630: UIBox (all of it) and UIElement from init to update_text.
#include "all.h"

#define NODE(x) (&(x)->moveable.node)
#define CFG(x) ((x)->moveable.node.config)

// ui.lua:2 UIBox = Moveable:extend()
// note: the entries left NULL are inherited from Moveable by class_init. The casts only change self's pointer type.
Class UIBox_class = {
    .name = "UIBox",
    .super = &Moveable_class,
    .draw = (void (*)(void *))UIBox_draw,
    .remove = (void (*)(void *))UIBox_remove,
    .move = (void (*)(void *, float))UIBox_move,
    .drag = (void (*)(void *, const Vec2 *))UIBox_drag,
    .set_container = (void (*)(void *, Node *))UIBox_set_container,
};

// ui.lua:348 UIElement = Moveable:extend()
// note: update, collides_with_point, click, put_focused_cursor, remove, hover, stop_hover and release are defined in
// the second part of the file (ui.lua:938 on).
Class UIElement_class = {
    .name = "UIElement",
    .super = &Moveable_class,
    .draw = (void (*)(void *))UIElement_draw,
    .update = (void (*)(void *, float))UIElement_update,
    .remove = (void (*)(void *))UIElement_remove,
    .hover = (void (*)(void *))UIElement_hover,
    .stop_hover = (void (*)(void *))UIElement_stop_hover,
    .click = (void (*)(void *))UIElement_click,
    .release = (void (*)(void *, void *))UIElement_release,
    .can_drag = (void *(*)(void *))UIElement_can_drag,
    .collides_with_point = (bool (*)(void *, Vec2))UIElement_collides_with_point,
    .juice_up = (void (*)(void *, float, Val))UIElement_juice_up,
    .put_focused_cursor = (void (*)(void *, float *, float *))UIElement_put_focused_cursor,
};

// ref_table[ref_value] of a UI binding (ui.h). No Lua line: the Lua indexes the table or the object directly
// (ui.lua:138, 398, 623, 632).
// note: weak, so that the build links whether or not the second part of ui.lua defines it too.
__attribute__((weak)) Val ui_ref_get(Val ref_table, Val ref_value) {
    if (ref_value.t == V_NIL) return NIL; // t[nil] reads as nil
    if (ref_table.t == V_TAB) return tgetv(ref_table.tab, ref_value);
    if (ref_table.t == V_OBJ) {
        void *obj = ref_table.obj;
        if (ref_value.t == V_STR) {
            if (is_a(obj, &Card_class)) return Card_field(obj, ref_value.s);
            if (is_a(obj, &Blind_class)) return Blind_field(obj, ref_value.s);
            if (is_a(obj, &DynaText_class) && streq(ref_value.s, "focused_string")) return N(((DynaText *)obj)->focused_string);
        }
        return tgetv(((Object *)obj)->dyn, ref_value); // G's setup_seed / run_setup_seed and anything else
    }
    return NIL;
}

// G.FUNCS[name](e)
static void call_func(const char *name, UIElement *e) {
    FuncsFn fn = funcs_lookup(name);
    if (!fn) ERROR("attempt to call field '%s' (a nil value)", name ? name : "?");
    fn(e);
}

static bool key_is(Val key, const char *name) { return key.t == V_STR && streq(key.s, name); }

// ui.lua:18
void UIBox_init(UIBox *self, Table *args) {
    Moveable *m = &self->moveable;
    Node *n = &m->node;
    Table *config = ttab(args, "config");

    // First initialize the moveable
    // note: {args.T} has no T key, so Node.init takes its defaults 0, 0, 1, 1 (the Lua's own behaviour)
    Moveable_init_args(m, ARR(tget(args, "T")));

    // Initialization of fields
    n->states.drag.can = false;
    self->draw_layers = tnew(); // if we need to explicitly change the draw order of the UIEs

    // The definition table that contains the schematic of this UIBox
    self->definition = ttab(args, "definition");

    if (config) {
        n->config = config;
        if (!truthy(tget(config, "major"))) {
            Val parent = tget(config, "parent");
            tset(config, "major", truthy(parent) ? parent : O(self));
        }

        Val type = tget(config, "align");
        if (!truthy(type)) type = tget(config, "type");
        if (!truthy(type)) type = S("");
        Val bond = tget(config, "bond");
        if (!truthy(bond)) bond = S("Strong");
        Val offset = tget(config, "offset");
        if (!truthy(offset)) offset = T(TAB("x", N(0), "y", N(0)));
        Moveable_set_alignment(m, TAB("major", tget(config, "major"),
                                      "type", type,
                                      "bond", bond,
                                      "offset", offset,
                                      "lr_clamp", tget(config, "lr_clamp")));
        Val wh_bond = tget(config, "wh_bond");
        if (!truthy(wh_bond)) wh_bond = S("Weak");
        Val scale_bond = tget(config, "scale_bond");
        if (!truthy(scale_bond)) scale_bond = S("Weak");
        Moveable_set_role(m, TAB("xy_bond", tget(config, "xy_bond"),
                                 "r_bond", tget(config, "r_bond"),
                                 "wh_bond", wh_bond,
                                 "scale_bond", scale_bond));
        n->states.collide.can = true;

        Val can_collide = tget(config, "can_collide");
        if (can_collide.t == V_NIL) {
            n->states.collide.can = true;
        } else {
            n->states.collide.can = truthy(can_collide);
        }

        n->parent = tobj(n->config, "parent");
    }

    // Initialization of the UIBox from the definition
    // First, set parent-child relationships to create the tree structure of the box
    UIBox_set_parent_child(self, self->definition, NULL);
    // Set the midpoint for any future alignments to use
    if (!m->Mid) m->Mid = (Moveable *)self->UIRoot;
    // Calculate the correct and width/height and offset for each node
    float w, h;
    UIBox_calculate_xywh(self, self->UIRoot, n->T, false, -1, &w, &h);

    // set the transform w/h to equal that of the calculated box
    n->T->w = NODE(self->UIRoot)->T->w;
    n->T->h = NODE(self->UIRoot)->T->h;
    // Then, calculate the correct width and height for each container
    UIElement_set_wh(self->UIRoot, &h);
    // Then, set all of the correct alignments for the ui elements
    UIElement_set_alignments(self->UIRoot);

    Moveable_align_to_major(m);
    m->VT.x = n->T->x, m->VT.y = n->T->y;
    m->VT.w = n->T->w, m->VT.h = n->T->h;

    // ui.lua:82-85: `if self.Mid ~= self and self.Mid.parent and false then`: never runs

    if (m->alignment.lr_clamp) {
        Moveable_lr_clamp(m);
    }

    UIElement_initialize_VT(self->UIRoot);
    // getmetatable(self) == UIBox: always (nothing extends UIBox)
    if (!config) ERROR("attempt to index field 'config' (a nil value)"); // ui.lua:93
    if (truthy(tget(config, "instance_type"))) {
        tinsert(I_get(tstr(config, "instance_type")), O(self));
    } else {
        tinsert(G->I.UIBOX, O(self));
    }
}

// ui.lua:101
UIElement *UIBox_get_UIE_by_ID(UIBox *self, Val id, Node *node) {
    if (!node) node = (Node *)self->UIRoot;
    if (node->config && veq(tget(node->config, "id"), id)) return (UIElement *)node;
    for (TIter it = titer(node->children); tnext(&it);) {
        Node *v = it.val.obj;
        UIElement *res = UIBox_get_UIE_by_ID(self, id, v);
        if (res) {
            return res;
        } else {
            void *object = tobj(v->config, "object");
            if (object && is_a(object, &UIBox_class)) { // object.get_UIE_by_ID
                res = UIBox_get_UIE_by_ID(object, id, NULL);
                if (res) {
                    return res;
                }
            }
        }
    }
    return NULL;
}

// ui.lua:118
bool UIBox_calculate_xywh(UIBox *self, UIElement *node, const Transform *_T, bool recalculate, float _scale, float *w, float *h) {
    Table *config = CFG(node);
    Transform _nt = {0}; // node.ARGS.xywh_node_trans
    Transform _ct = {0};

    Val vpadding = tget(config, "padding");
    float padding = truthy(vpadding) ? (float)vnum(vpadding) : UIT_padding;
    // current node does not contain anything
    if (node->UIT == UIT_B || node->UIT == UIT_T || node->UIT == UIT_O) {
        Val cw = tget(config, "w"), ch = tget(config, "h");
        Moveable *object = tobj(config, "object");
        // note: a nil w or h (a B with no size, an O with no object) is has_w / has_h false
        bool has_w = truthy(cw) || object, has_h = truthy(ch) || object;
        _nt.x = _T->x;
        _nt.y = _T->y;
        _nt.w = truthy(cw) ? (float)vnum(cw) : object ? object->node.T->w : 0;
        _nt.h = truthy(ch) ? (float)vnum(ch) : object ? object->node.T->h : 0;

        if (node->UIT == UIT_T) {
            // note: the Lua drops the drawable for its collector; here it is released
            Val drawable = tget(config, "text_drawable");
            if (drawable.t == V_PTR && drawable.p) Text_release(drawable.p);
            tset(config, "text_drawable", NIL);
            Val vscale = tget(config, "scale");
            float scale = truthy(vscale) ? (float)vnum(vscale) : 1;
            if (truthy(tget(config, "ref_table")) && truthy(tget(config, "ref_value"))) {
                tsets(config, "text", tostring(ui_ref_get(tget(config, "ref_table"), tget(config, "ref_value"))));
                if (truthy(tget(config, "func")) && !recalculate) call_func(tstr(config, "func"), node);
            }
            if (!truthy(tget(config, "text"))) tsets(config, "text", "[UI ERROR]");
            if (!truthy(tget(config, "lang"))) tsett(config, "lang", G->LANG);
            Table *font = ttab(ttab(config, "lang"), "font");
            float tx = Font_getWidth(font_FONT(font), vstr(tget(config, "text"))) * (float)tnum(font, "squish") * scale * G->TILESCALE * (float)tnum(font, "FONTSCALE");
            float ty = Font_getHeight(font_FONT(font)) * scale * G->TILESCALE * (float)tnum(font, "FONTSCALE") * (float)tnum(font, "TEXT_HEIGHT_SCALE");
            if (tbool(config, "vert")) { float thunk = tx; tx = ty; ty = thunk; }
            _nt.x = _T->x;
            _nt.y = _T->y;
            _nt.w = tx / (G->TILESIZE * G->TILESCALE);
            _nt.h = ty / (G->TILESIZE * G->TILESCALE);
            has_w = has_h = true;

            node->content_dimensions.w = _T->w;
            node->content_dimensions.h = _T->h;
            UIElement_set_values(node, &_nt, recalculate);
        } else if (node->UIT == UIT_B || node->UIT == UIT_O) {
            // note: a nil size is 0 in content_dimensions and Node.init's default 1 in the transform given to
            // set_values (on a recalculate the Lua writes the nil into T; a Transform cannot hold it: ui.h)
            node->content_dimensions.w = _nt.w;
            node->content_dimensions.h = _nt.h;
            Transform _st = _nt;
            if (!has_w) _st.w = 1;
            if (!has_h) _st.h = 1;
            UIElement_set_values(node, &_st, recalculate);
        }
        *w = _nt.w, *h = _nt.h;
        return has_w && has_h;
    } else { // For all other node containers, treat them explicitly like a column
        for (int i = 1; i <= 2; i++) {
            Val maxw = tget(config, "maxw"), maxh = tget(config, "maxh");
            if (i == 1 || (i == 2 && ((truthy(maxw) && _ct.w > vnum(maxw)) || (truthy(maxh) && _ct.h > vnum(maxh))))) {
                float fac = _scale < 0 ? 1 : _scale;
                if (i == 2) {
                    num restriction = truthy(maxw) ? vnum(maxw) : vnum(maxh);
                    fac = (float)(fac * restriction / (truthy(maxw) ? _ct.w : _ct.h));
                }
                Val minw = tget(config, "minw"), minh = tget(config, "minh");
                _nt.x = _T->x;
                _nt.y = _T->y;
                _nt.w = truthy(minw) ? (float)vnum(minw) : 0;
                _nt.h = truthy(minh) ? (float)vnum(minh) : 0;

                if (node->UIT == UIT_ROOT) {
                    _nt.x = 0, _nt.y = 0;
                    _nt.w = truthy(minw) ? (float)vnum(minw) : 0;
                    _nt.h = truthy(minh) ? (float)vnum(minh) : 0;
                }
                _ct.x = _nt.x + padding, _ct.y = _nt.y + padding, _ct.w = 0, _ct.h = 0;
                float _tw, _th;
                Table *children = NODE(node)->children;
                for (int k = 1; tgeti(children, k).t != V_NIL; k++) { // ipairs
                    Val vv = tgeti(children, k);
                    if (vv.t == V_OBJ && ((Object *)vv.obj)->cls == &UIElement_class) { // getmetatable(v) == UIElement
                        UIElement *v = vv.obj;
                        Table *vc = CFG(v);
                        if (vc && truthy(tget(vc, "scale"))) tsetn(vc, "scale", tnum(vc, "scale") * fac);
                        if (UIBox_calculate_xywh(self, v, &_ct, recalculate, fac, &_tw, &_th)) { // if _th and _tw
                            Val emboss = tget(vc, "emboss");
                            if (v->UIT == UIT_R) {
                                _ct.h = _ct.h + _th + padding;
                                _ct.y = _ct.y + _th + padding;
                                if (_tw + padding > _ct.w) _ct.w = _tw + padding;
                                if (vc && truthy(emboss)) {
                                    _ct.h = _ct.h + (float)vnum(emboss);
                                    _ct.y = _ct.y + (float)vnum(emboss);
                                }
                            } else {
                                _ct.w = _ct.w + _tw + padding;
                                _ct.x = _ct.x + _tw + padding;
                                if (_th + padding > _ct.h) _ct.h = _th + padding;
                                if (vc && truthy(emboss)) {
                                    _ct.h = _ct.h + (float)vnum(emboss);
                                }
                            }
                        }
                    }
                }
            }
        }

        node->content_dimensions.w = _ct.w + padding;
        node->content_dimensions.h = _ct.h + padding;
        _nt.w = fmaxf(_ct.w + padding, _nt.w);
        _nt.h = fmaxf(_ct.h + padding, _nt.h);
        UIElement_set_values(node, &_nt, recalculate);
        *w = _nt.w, *h = _nt.h;
        return true;
    }
}

// ui.lua:219
bool UIBox_remove_group(UIBox *self, Node *node, Val group) {
    if (!node) node = (Node *)self->UIRoot;
    // note: walks a shallow copy, since the loop clears entries of node.children
    for (TIter it = titer(tshallow(node->children)); tnext(&it);) {
        if (UIBox_remove_group(self, it.val.obj, group)) tsetv(node->children, it.key, NIL);
    }
    if (node->config && truthy(tget(node->config, "group")) && veq(tget(node->config, "group"), group)) {
        CALL(node, remove);
        return true;
    }

    // `if not node.parent or true then`
    float w, h;
    UIBox_calculate_xywh(self, self->UIRoot, self->moveable.node.T, true, -1, &w, &h);
    UIElement_set_wh(self->UIRoot, &h);
    Moveable_set_alignment((Moveable *)self->UIRoot, NULL); // self.UIRoot:set_alignment() (the Lua's spelling)
    return false;
}

// ui.lua:229
Table *UIBox_get_group(UIBox *self, Node *node, Val group, Table *ingroup) {
    if (!node) node = (Node *)self->UIRoot;
    if (!ingroup) ingroup = tnew();
    for (TIter it = titer(node->children); tnext(&it);) {
        UIBox_get_group(self, it.val.obj, group, ingroup);
    }
    if (node->config && truthy(tget(node->config, "group")) && veq(tget(node->config, "group"), group)) {
        tinsert(ingroup, O(node));
        return ingroup;
    }
    return ingroup;
}

// ui.lua:239
void UIBox_set_parent_child(UIBox *self, Table *node, UIElement *parent) {
    Val vn = tget(node, "n");
    UIType n = (UIType)(int)vnum(vn);
    UIElement *UIE = NEW(UIElement);
    UIElement_init(UIE, (Node *)parent, self, n, ttab(node, "config"));
    Table *pconfig = parent ? CFG(parent) : NULL;
    // note: UIE.config always exists (UIElement:init), so the Lua's `else UIE.config = {...}` branches cannot run

    // set the group of the element
    if (parent && pconfig && truthy(tget(pconfig, "group"))) tset(CFG(UIE), "group", tget(pconfig, "group"));

    // set the button for the element
    if (parent && pconfig && truthy(tget(pconfig, "button"))) tseto(CFG(UIE), "button_UIE", parent);
    if (parent && pconfig && truthy(tget(pconfig, "button_UIE"))) tset(CFG(UIE), "button_UIE", tget(pconfig, "button_UIE"));

    if (truthy(vn) && n == UIT_O && truthy(tget(CFG(UIE), "button"))) {
        // note: a Card's sprites share its states table in the Lua; card.h says who mirrors the flags
        ((Node *)tobj(CFG(UIE), "object"))->states.click.can = false;
    }

    // current node is a container
    if (((truthy(vn) && n == UIT_C) || n == UIT_R || n == UIT_ROOT) && truthy(tget(node, "nodes"))) {
        for (TIter it = titer(ttab(node, "nodes")); tnext(&it);) { // pairs: the nil holes are skipped
            if (it.val.t != V_TAB) continue; // note: a hole kept as a non-table value
            UIBox_set_parent_child(self, it.val.tab, UIE);
        }
    }

    if (!parent) {
        self->UIRoot = UIE;
        NODE(self->UIRoot)->parent = (Node *)self;
    } else {
        tinsert(NODE(parent)->children, O(UIE));
    }
    if (truthy(tget(node, "config")) && tbool(ttab(node, "config"), "mid")) {
        self->moveable.Mid = (Moveable *)UIE;
    }
}

// ui.lua:270
void UIBox_remove(UIBox *self) {
    Node *n = &self->moveable.node;
    if (self == G->OVERLAY_MENU) G->REFRESH_ALERTS = true;
    UIElement_remove(self->UIRoot);
    const char *instance_type = truthy(tget(n->config, "instance_type")) ? tstr(n->config, "instance_type") : "UIBOX";
    Table *list = I_get(instance_type);
    for (int k = 1, len = tlen(list); k <= len; k++) {
        if (tgeti(list, k).obj == self) {
            tremove(list, k);
            break;
        }
    }
    remove_all(n->children);
    Moveable_remove(&self->moveable);
}

// ui.lua:283
void UIBox_draw(UIBox *self) {
    Node *n = &self->moveable.node;
    if (n->FRAME.DRAW >= G->FRAMES.DRAW && !G->OVERLAY_TUTORIAL) return;
    n->FRAME.DRAW = G->FRAMES.DRAW;

    for (TIter it = titer(n->children); tnext(&it);) {
        if (!key_is(it.key, "h_popup") && !key_is(it.key, "alert")) CALL(it.val.obj, draw);
    }

    if (n->states.visible) {
        add_to_drawhash(self);
        UIElement_draw_self(self->UIRoot);
        UIElement_draw_children(self->UIRoot);
        for (int k = 1; tgeti(self->draw_layers, k).t != V_NIL; k++) { // ipairs
            void *v = tgeti(self->draw_layers, k).obj;
            bool uie = is_a(v, &UIElement_class); // v.draw_self / v.draw_children (ui.h)
            if (uie) UIElement_draw_self(v); else CALL(v, draw);
            if (uie) UIElement_draw_children(v);
        }
    }

    void *alert = tobj(n->children, "alert");
    if (alert) CALL(alert, draw);

    Node_draw_boundingrect(n);
}

// ui.lua:306
void UIBox_recalculate(UIBox *self) {
    Node *n = &self->moveable.node;
    float w, h;
    // Calculate the correct dimensions and width/height and offset for each node
    UIBox_calculate_xywh(self, self->UIRoot, n->T, true, -1, &w, &h);
    // Then, calculate the correct width and height for each container
    UIElement_set_wh(self->UIRoot, &h);
    // Then, set all of the correct alignments for the ui elements
    UIElement_set_alignments(self->UIRoot);
    n->T->w = NODE(self->UIRoot)->T->w;
    n->T->h = NODE(self->UIRoot)->T->h;
    G->REFRESH_FRAME_MAJOR_CACHE = G->REFRESH_FRAME_MAJOR_CACHE + 1; // (0 as nil)
    UIElement_initialize_VT(self->UIRoot);
    G->REFRESH_FRAME_MAJOR_CACHE = G->REFRESH_FRAME_MAJOR_CACHE > 1 ? G->REFRESH_FRAME_MAJOR_CACHE - 1 : 0;
}

// ui.lua:320
void UIBox_move(UIBox *self, float dt) {
    Moveable_move(&self->moveable, dt);
    Moveable_move((Moveable *)self->UIRoot, dt);
}

// ui.lua:325
void UIBox_drag(UIBox *self, const Vec2 *offset) {
    Moveable_drag(&self->moveable, offset);
    // note: the Lua passes an undefined global dt (nil); the root is a Minor, whose move does not use dt: 0
    Moveable_move((Moveable *)self->UIRoot, 0);
}

// ui.lua:330
void UIBox_add_child(UIBox *self, Table *node, UIElement *parent) {
    UIBox_set_parent_child(self, node, parent);
    UIBox_recalculate(self);
}

// ui.lua:335
void UIBox_set_container(UIBox *self, Node *container) {
    CALL(self->UIRoot, set_container, container);
    Node_set_container(&self->moveable.node, container);
}

// ui.lua:340
const char *UIBox_print_topology(UIBox *self, int indent) {
    Node *n = &self->moveable.node;
    const char *box_str = fmt("| UIBox | - ID:%d w/h:%s/%s", n->ID, number_tostring(n->T->w), number_tostring(n->T->h));
    box_str = fmt("%s%s", box_str, UIElement_print_topology(self->UIRoot, indent));
    return box_str;
}

// ui.lua:350
void UIElement_init(UIElement *self, Node *parent, UIBox *new_UIBox, UIType new_UIT, Table *config) {
    Node *n = NODE(self);
    n->parent = parent;
    self->UIT = new_UIT;
    self->UIBox = new_UIBox;
    n->config = config ? config : tnew();
    config = n->config;
    if (truthy(tget(config, "object"))) ((Node *)tobj(config, "object"))->parent = (Node *)self;
    n->children = tnew();
    // self.ARGS: scratch, does not exist here
    self->content_dimensions.w = 0, self->content_dimensions.h = 0;

    // The copies of the config keys nothing assigns after creation (ui.h)
    Val v;
    self->cfg.draw_layer = truthy(v = tget(config, "draw_layer")) ? (int)vnum(v) : 0;
    self->cfg.force_collision = tbool(config, "force_collision");
    self->cfg.button_dist = truthy(v = tget(config, "button_dist")) ? (float)vnum(v) : 1;
    self->cfg.vert = tbool(config, "vert");
    self->cfg.shadow_colour = UI_colour(config, "shadow_colour");
    self->cfg.r = tbool(config, "r");
    self->cfg.has_emboss = truthy(v = tget(config, "emboss"));
    self->cfg.emboss = (float)vnum(v);
    self->cfg.focus_with_object = tbool(config, "focus_with_object");
    self->cfg.has_line_emboss = truthy(v = tget(config, "line_emboss"));
    self->cfg.line_emboss = (float)vnum(v);
    self->cfg.ext_up = truthy(v = tget(config, "ext_up")) ? (float)vnum(v) : 0;
    self->cfg.has_res = truthy(v = tget(config, "res"));
    self->cfg.res = (float)vnum(v);
    self->cfg.padding = truthy(v = tget(config, "padding")) ? (float)vnum(v) : UIT_padding;
    self->cfg.has_h = truthy(v = tget(config, "h"));
    self->cfg.h = (float)vnum(v);
    self->cfg.has_maxw = truthy(v = tget(config, "maxw"));
    self->cfg.maxw = (float)vnum(v);
    self->cfg.has_maxh = truthy(v = tget(config, "maxh"));
    self->cfg.maxh = (float)vnum(v);
    self->cfg.minw = truthy(v = tget(config, "minw")) ? (float)vnum(v) : 0;
    self->cfg.no_recalc = tbool(config, "no_recalc");
    self->cfg.ref_value = tget(config, "ref_value");
    self->cfg.progress_bar = ttab(config, "progress_bar");
    self->cfg.role = ttab(config, "role");
    self->cfg.no_role = tbool(config, "no_role");
}

// ui.lua:360
void UIElement_set_values(UIElement *self, const Transform *_T, bool recalculate) {
    Moveable *m = &self->moveable;
    Node *n = &m->node;
    Table *config = n->config;
    if (!recalculate || !n->T) {
        Moveable_init_args(m, TAB("T", T(TAB("x", N(_T->x), "y", N(_T->y), "w", N(_T->w), "h", N(_T->h)))));
        n->states.click.can = false;
        n->states.drag.can = false;
        m->static_rotation = true;
    } else {
        n->T->x = _T->x;
        n->T->y = _T->y;
        n->T->w = _T->w;
        n->T->h = _T->h;
    }

    if (truthy(tget(config, "button_UIE"))) { n->states.collide.can = true; n->states.hover.can = false; n->states.click.can = true; }
    if (truthy(tget(config, "button"))) { n->states.collide.can = true; n->states.click.can = true; }

    if (truthy(tget(config, "on_demand_tooltip")) || truthy(tget(config, "tooltip")) || truthy(tget(config, "detailed_tooltip"))) {
        n->states.collide.can = true;
    }

    Moveable_set_role(m, TAB("role_type", S("Minor"), "major", O(self->UIBox), "offset", T(TAB("x", N(_T->x), "y", N(_T->y))), "wh_bond", S("Weak"), "scale_bond", S("Weak")));

    if (truthy(tget(config, "draw_layer"))) {
        tsetv(self->UIBox->draw_layers, tget(config, "draw_layer"), O(self));
    }

    if (truthy(tget(config, "collideable"))) n->states.collide.can = true;

    Val can_collide = tget(config, "can_collide");
    if (can_collide.t != V_NIL) {
        n->states.collide.can = truthy(can_collide);
        if (truthy(tget(config, "object"))) ((Node *)tobj(config, "object"))->states.collide.can = n->states.collide.can;
    }

    if (self->UIT == UIT_O && !truthy(tget(config, "no_role"))) {
        Table *role = ttab(config, "role");
        if (!role) role = TAB("role_type", S("Minor"), "major", O(self), "xy_bond", S("Strong"), "wh_bond", S("Weak"), "scale_bond", S("Weak"));
        Moveable_set_role(tobj(config, "object"), role);
    }

    if (config && truthy(tget(config, "ref_value")) && truthy(tget(config, "ref_table"))) {
        tset(config, "prev_value", ui_ref_get(tget(config, "ref_table"), tget(config, "ref_value")));
    }

    if (self->UIT == UIT_T) m->static_rotation = true;

    if (truthy(tget(config, "juice"))) {
        if (self->UIT == UIT_ROOT) UIElement_juice_up(self, -1, NIL);
        if (self->UIT == UIT_T) UIElement_juice_up(self, -1, NIL);
        if (self->UIT == UIT_O) CALL(tobj(config, "object"), juice_up, 0.5f, NIL);
        if (self->UIT == UIT_B) UIElement_juice_up(self, -1, NIL);
        if (self->UIT == UIT_C) UIElement_juice_up(self, -1, NIL);
        if (self->UIT == UIT_R) UIElement_juice_up(self, -1, NIL);
        tsetb(config, "juice", false);
    }

    if (!truthy(tget(config, "colour"))) {
        if (self->UIT == UIT_ROOT) tset(config, "colour", P(G->C.UI.BACKGROUND_DARK));
        if (self->UIT == UIT_T) tset(config, "colour", P(G->C.UI.TEXT_LIGHT));
        if (self->UIT == UIT_O) tset(config, "colour", P(G->C.WHITE));
        if (self->UIT == UIT_B) tset(config, "colour", P(G->C.CLEAR));
        if (self->UIT == UIT_C) tset(config, "colour", P(G->C.CLEAR));
        if (self->UIT == UIT_R) tset(config, "colour", P(G->C.CLEAR));
    }
    if (!truthy(tget(config, "outline_colour"))) {
        if (self->UIT == UIT_ROOT) tset(config, "outline_colour", P(G->C.UI.OUTLINE_LIGHT));
        if (self->UIT == UIT_T) tset(config, "outline_colour", P(G->C.UI.OUTLINE_LIGHT));
        if (self->UIT == UIT_O) tset(config, "colour", P(G->C.UI.OUTLINE_LIGHT)); // (the Lua writes colour here)
        if (self->UIT == UIT_B) tset(config, "outline_colour", P(G->C.UI.OUTLINE_LIGHT));
        if (self->UIT == UIT_C) tset(config, "outline_colour", P(G->C.UI.OUTLINE_LIGHT));
        if (self->UIT == UIT_R) tset(config, "outline_colour", P(G->C.UI.OUTLINE_LIGHT));
    }

    Table *focus_args = ttab(config, "focus_args");
    if (focus_args && !tbool(focus_args, "registered")) {
        if (truthy(tget(focus_args, "button"))) {
            Node *button_UIE = tobj(config, "button_UIE");
            Controller_add_to_registry(G->CONTROLLER, button_UIE ? button_UIE : n, tstr(focus_args, "button"));
        }

        if (truthy(tget(focus_args, "snap_to"))) {
            Controller_snap_to(G->CONTROLLER, TAB("node", O(self)));
        }

        if (truthy(tget(focus_args, "funnel_to"))) {
            Node *_par = n->parent;
            while (_par && is_a(_par, &UIElement_class)) {
                Table *par_focus_args = ttab(_par->config, "focus_args");
                if (par_focus_args && truthy(tget(par_focus_args, "funnel_from"))) {
                    tseto(par_focus_args, "funnel_from", self);
                    tseto(focus_args, "funnel_to", _par);
                    break;
                }
                _par = _par->parent;
            }
        }
        tsetb(focus_args, "registered", true);
    }

    if (truthy(tget(config, "force_focus"))) n->states.collide.can = true;

    if (truthy(tget(config, "button_delay")) && !truthy(tget(config, "button_delay_start"))) {
        tsetn(config, "button_delay_start", G->TIMERS.REAL);
        tsetn(config, "button_delay_end", G->TIMERS.REAL + tnum(config, "button_delay"));
        tsetn(config, "button_delay_progress", 0);
    }

    // self.layered_parallax = self.layered_parallax or {x=0, y=0}: a Vec2, always there

    if (config && truthy(tget(config, "func")) && (((truthy(tget(config, "button_UIE")) || truthy(tget(config, "button"))) && !streq(tstr(config, "func"), "set_button_pip")) || truthy(tget(config, "insta_func")))) {
        call_func(tstr(config, "func"), self);
    }
}

// ui.lua:466
const char *UIElement_print_topology(UIElement *self, int indent) {
    Node *n = NODE(self);
    const char *UIT = "????";
    switch (self->UIT) { // for k, v in pairs(G.UIT)
    case UIT_T: UIT = "T"; break;
    case UIT_B: UIT = "B"; break;
    case UIT_C: UIT = "C"; break;
    case UIT_R: UIT = "R"; break;
    case UIT_O: UIT = "O"; break;
    case UIT_ROOT: UIT = "ROOT"; break;
    case UIT_S: UIT = "S"; break;
    case UIT_I: UIT = "I"; break;
    }
    const char *box_str = fmt("\n%s| %s | - ID:%d w/h:%s/%s", str_rep("  ", indent), UIT, n->ID, number_tostring(n->T->w), number_tostring(n->T->h));
    if (streq(UIT, "O")) {
        Object *object = tobj(n->config, "object");
        const Class *cls = object ? object->cls : NULL; // getmetatable(self.config.object)
        box_str = fmt("%s OBJ:%s", box_str,
                      cls == &CardArea_class ? "CardArea" :
                      cls == &Card_class ? "Card" :
                      cls == &UIBox_class ? "UIBox" :
                      cls == &Particles_class ? "Particles" :
                      cls == &DynaText_class ? "DynaText" :
                      cls == &Sprite_class ? "Sprite" :
                      cls == &AnimatedSprite_class ? "AnimatedSprite" :
                      "OTHER");
    } else if (streq(UIT, "T")) {
        Val text = tget(n->config, "text");
        box_str = fmt("%s TEXT:%s", box_str, truthy(text) ? vstr(text) : "REF");
    }

    for (int k = 1; tgeti(n->children, k).t != V_NIL; k++) { // ipairs
        void *v = tgeti(n->children, k).obj;
        if (is_a(v, &UIElement_class)) { // v.print_topology
            box_str = fmt("%s%s", box_str, UIElement_print_topology(v, indent + 1));
        }
    }
    return box_str;
}

// ui.lua:495
void UIElement_initialize_VT(UIElement *self) {
    Moveable *m = &self->moveable;
    Node *n = &m->node;
    Moveable_move_with_major(m, 0);
    Moveable_calculate_parrallax(m);

    for (TIter it = titer(n->children); tnext(&it);) {
        if (is_a(it.val.obj, &UIElement_class)) UIElement_initialize_VT(it.val.obj); // v.initialize_VT
    }

    m->VT.w = n->T->w, m->VT.h = n->T->h;

    if (self->UIT == UIT_T) UIElement_update_text(self);
    Moveable *object = tobj(n->config, "object");
    if (object) {
        if (!truthy(tget(n->config, "no_role"))) {
            CALL(object, hard_set_T, n->T->x, n->T->y, n->T->w, n->T->h);
            Moveable_move_with_major(object, 0);
            object->alignment.prev_type = "";
            Moveable_align_to_major(object);
        }
        if (is_a(object, &UIBox_class)) { // object.recalculate
            UIBox_recalculate((UIBox *)object);
        }
    }
}

// ui.lua:519
void UIElement_juice_up(UIElement *self, float amount, Val rot_amt) {
    if (self->UIT == UIT_O) {
        void *object = tobj(CFG(self), "object");
        if (object) CALL(object, juice_up, amount, rot_amt);
    } else {
        Moveable_juice_up(&self->moveable, amount, rot_amt);
    }
}

// ui.lua:527
Node *UIElement_can_drag(UIElement *self) {
    if (NODE(self)->states.drag.can) return NODE(self);
    return CALL(self->UIBox, can_drag);
}

// ui.lua:532
void UIElement_draw(UIElement *self) {
}

// ui.lua:535
void UIElement_draw_children(UIElement *self) {
    Node *n = NODE(self);
    if (n->states.visible) {
        for (TIter it = titer(n->children); tnext(&it);) {
            Node *v = it.val.obj;
            bool uie = is_a(v, &UIElement_class); // v.draw_self / v.draw_children (ui.h)
            // (an element's config.draw_layer is its cfg.draw_layer: ui.h)
            if (!(uie ? ((UIElement *)v)->cfg.draw_layer != 0 : truthy(tget(v->config, "draw_layer"))) && !key_is(it.key, "h_popup") && !key_is(it.key, "alert")) {
                // config.draw_after is read once: only button callbacks assign it (button_callbacks.lua:2646-2675),
                // nothing that runs inside a draw
                bool draw_after = uie && UI_snap((UIElement *)v)->draw_after;
                if (uie && !draw_after) UIElement_draw_self((UIElement *)v); else CALL(v, draw);
                if (uie) UIElement_draw_children((UIElement *)v);
                if (uie && draw_after) UIElement_draw_self((UIElement *)v); else CALL(v, draw);
            }
        }
    }
}

// ui.lua:547
float UIElement_set_wh(UIElement *self, float *h) {
    Node *n = NODE(self);
    // Iterate through all children of this node
    Val vpadding = n->config ? tget(n->config, "padding") : NIL;
    float padding = truthy(vpadding) ? (float)vnum(vpadding) : UIT_padding;

    float _max_w = 0, _max_h = 0;

    if (tempty(n->children) || truthy(tget(n->config, "no_fill"))) {
        *h = n->T->h;
        return n->T->w;
    } else {
        for (TIter it = titer(n->children); tnext(&it);) {
            if (is_a(it.val.obj, &UIElement_class)) { // w.set_wh
                float _cw, _ch;
                _cw = UIElement_set_wh(it.val.obj, &_ch);
                // note: `if _cw and _ch` always holds: a Transform cannot hold the nil size of ui.h's note, so the
                // Lua's else (_max_w = padding; _max_h = padding) cannot run
                if (_cw > _max_w) _max_w = _cw;
                if (_ch > _max_h) _max_h = _ch;
            }
        }
        for (TIter it = titer(n->children); tnext(&it);) {
            if (!is_a(it.val.obj, &UIElement_class)) continue; // w.UIT is nil for anything else
            UIElement *w = it.val.obj;
            if (w->UIT == UIT_R) NODE(w)->T->w = _max_w;
            if (w->UIT == UIT_C) NODE(w)->T->h = _max_h;
        }
    }
    *h = n->T->h;
    return n->T->w;
}

// ui.lua:576
void UIElement_align(UIElement *self, float x, float y) {
    self->moveable.role.offset.y = self->moveable.role.offset.y + y;
    self->moveable.role.offset.x = self->moveable.role.offset.x + x;
    for (TIter it = titer(NODE(self)->children); tnext(&it);) {
        if (is_a(it.val.obj, &UIElement_class)) { // v.align
            UIElement_align(it.val.obj, x, y);
        }
    }
}

// ui.lua:586
void UIElement_set_alignments(UIElement *self) {
    Node *n = NODE(self);
    // vertically centered is c = centered
    // horizontally centered is m = middle
    // top and left are default
    // bottom is b
    // right is r
    for (TIter it = titer(n->children); tnext(&it);) {
        bool uie = is_a(it.val.obj, &UIElement_class);
        UIElement *v = it.val.obj;
        if (n->config && truthy(tget(n->config, "align")) && uie) { // v.align
            Val vpadding = tget(n->config, "padding");
            float padding = truthy(vpadding) ? (float)vnum(vpadding) : UIT_padding;
            const char *align = tstr(n->config, "align");

            if (str_find(align, "c", 1)) {
                if (v->UIT == UIT_T || v->UIT == UIT_B || v->UIT == UIT_O) {
                    UIElement_align(v, 0, 0.5f * (n->T->h - 2 * padding - NODE(v)->T->h));
                } else {
                    UIElement_align(v, 0, 0.5f * (n->T->h - self->content_dimensions.h));
                }
            }
            if (str_find(align, "m", 1)) {
                UIElement_align(v, 0.5f * (n->T->w - self->content_dimensions.w), 0);
            }
            if (str_find(align, "b", 1)) {
                UIElement_align(v, 0, n->T->h - self->content_dimensions.h);
            }
            if (str_find(align, "r", 1)) {
                UIElement_align(v, (n->T->w - self->content_dimensions.w), 0);
            }
        }
        if (uie) UIElement_set_alignments(v); // v.set_alignments
    }
}

// ui.lua:617
void UIElement_update_text(UIElement *self) {
    Table *config = CFG(self);
    const struct UISnap *s = UI_snap(self); // (ui.h: the table is read again only after a write to it)
    // Nothing to do while neither config nor the bound table was written since the last look found the text
    // up to date (a number index is in the array part, which a stamp does not cover: always looked at)
    if (truthy(s->text_drawable) && s->ref_table.t == V_TAB && self->idle_ref == s->ref_table.tab &&
        self->idle_ref_stamp == tstamp(self->idle_ref) && self->idle_cfg_stamp == s->stamp) return;
    if (!truthy(s->ref_table) && (!truthy(s->text) || truthy(s->text_drawable))) return; // neither `if` below holds
    if (config && truthy(tget(config, "text")) && !truthy(tget(config, "text_drawable"))) {
        if (!truthy(tget(config, "lang"))) tsett(config, "lang", G->LANG);
        // {G.C.WHITE, text}: a coloured text's colours are dropped (love.h)
        tset(config, "text_drawable", P(love_graphics_newText(font_FONT(ttab(ttab(config, "lang"), "font")), vstr(tget(config, "text")))));
    }

    Val ref_table = tget(config, "ref_table");
    if (!(truthy(ref_table) && !veq(ui_ref_get(ref_table, tget(config, "ref_value")), tget(config, "prev_value")))) {
        if (ref_table.t == V_TAB && tget(config, "ref_value").t == V_STR) {
            self->idle_ref = ref_table.tab, self->idle_ref_stamp = tstamp(ref_table.tab), self->idle_cfg_stamp = tstamp(config);
        }
    } else {
        Val prev_value = tget(config, "prev_value");
        tsets(config, "text", tostring(ui_ref_get(tget(config, "ref_table"), tget(config, "ref_value"))));
        Text_set(tget(config, "text_drawable").p, tstr(config, "text"));
        if (!truthy(tget(config, "no_recalc")) && truthy(prev_value) && str_len(vstr(prev_value)) != str_len(tstr(config, "text"))) UIBox_recalculate(self->UIBox);
        tset(config, "prev_value", ui_ref_get(tget(config, "ref_table"), tget(config, "ref_value")));
    }
}
