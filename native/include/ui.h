// engine/ui.lua (UIBox, UIElement) and engine/text.lua (DynaText): the node-based UI and the animated text.
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is in
// obj.dyn. The scratch tables self.ARGS (xywh_node_trans, text_parallax, button_colours, draw_shadow_norm) do not
// exist: use locals.
//
// How the UI's data is kept
// - A UI definition stays a nested Table exactly as the Lua writes it: {n = N(UIT_R), config = T({...}),
//   nodes = T(ARR(...))}. 'nodes' may have nil holes (conditional entries): the Lua walks it with pairs, so walk it
//   with titer (ascending, holes skipped), not with a for to tlen.
// - A UIElement's config is the definition's config table itself (NOT a copy; shared and changed in place), in
//   Node.config (self->moveable.node.config). Same keys as the Lua. What the values are:
//     colour, outline_colour, shadow_colour, and every other colour: P(float *) to someone's 4 floats (the
//       convention of node.h and card.h); read one with UI_colour(config, "colour").
//     object: O(a Moveable of any class). button_UIE: O(UIElement). text_drawable: P(Text *).
//     button, func: S(name of a G.FUNCS entry). ref_table: T(table) or O(object); ref_value: S(key) or N(index).
//     focus_args, progress_bar, role, tooltip, on_demand_tooltip, detailed_tooltip, h_popup, h_popup_config: T(table).
//     focus_args.funnel_to / funnel_from: B(true) in the definition, then O(UIElement) once set_values links them.
//     chosen: B(bool) or S("vert"). lang: what game.h makes of G.LANG (the Lua reads lang.font.<FONT, FONTSCALE,
//       squish, TEXT_OFFSET.x/.y, TEXT_HEIGHT_SCALE>).
//     prev_value: the raw Val of ref_table[ref_value] (compare with veq, never as strings).
// - self.children is Node.children (a Table): UIElements at 1..n (O(UIElement)) and, by name, the 'h_popup' and
//   'alert' UIBoxes that Node_hover and others hang there. self.parent is Node.parent: the parent UIElement, or the
//   UIBox for the root.
// - The Lua's duck-typing tests on a child or an object: v.draw_self, v.draw_children, v.set_wh, v.align,
//   v.set_alignments, v.initialize_VT, v.print_topology and getmetatable(v) == UIElement are
//   is_a(v, &UIElement_class); object.recalculate and object.get_UIE_by_ID are is_a(object, &UIBox_class). No
//   class extends UIBox, UIElement or DynaText, so getmetatable(self) == UIBox / DynaText is always true.
// - self.T is nil until Moveable.init runs (a UIElement before its first set_values, a DynaText during the
//   update_text calls of its init): test it as self->moveable.node.T == NULL (NEW() zeroes it; Node_init sets it).
// - Time stamps taken from G.TIMERS.REAL are num (differences of large times).
#ifndef UI_H
#define UI_H
#include "types.h"
#include "node.h"

// ---------------------------------------------------------------- G.UIT (globals.lua:476)
typedef enum {
    UIT_T = 1,    // text
    UIT_B = 2,    // box (can be rounded)
    UIT_C = 3,    // column
    UIT_R = 4,    // row
    UIT_O = 5,    // object: config.object is a Node
    UIT_ROOT = 7,
    UIT_S = 8,    // slider (unused by ui.lua)
    UIT_I = 9     // input text box (unused by ui.lua)
} UIType;
#define UIT_padding 0.0f // G.UIT.padding: the default padding
// In a definition table 'n' is N(UIT_x): (UIType)tnum(node, "n").

extern Class UIBox_class, UIElement_class, DynaText_class;
// The Class entries (node.h) each class fills, the rest inherited from Moveable:
//   UIBox:     draw, remove, move, drag, set_container
//   UIElement: draw (empty), update, remove, hover, stop_hover, click, release, can_drag, collides_with_point,
//              juice_up, put_focused_cursor. NOT draw_self: that entry is the sprites' (self, overlay); call
//              UIElement_draw_self / UIElement_draw_children by name after is_a(v, &UIElement_class).
//   DynaText:  update, draw

// A colour kept in a table (config.colour...): the float[4] it points to, or NULL as nil.
// (a macro, so that a literal key is interned once per call site as tget's are: core.h)
static inline float *UI_colour_of(Val v) { return v.t == V_PTR ? (float *)v.p : NULL; }
#define UI_colour(config, key) UI_colour_of(tget(config, key))
// ref_table[ref_value] of a UI binding (a T node's text, an O node's object, progress_bar, a DynaText string part).
// ref_table: T(table) -> tgetv; O(object) -> that object's field by name: its class's own accessor where the field
// is a struct member (Card_field of card.h for 'cost' / 'sell_cost_label'; the game also binds G.GAME.blind
// 'chip_text' / 'loc_name', a DynaText's 'focused_string', G 'setup_seed' / 'run_setup_seed'), else its dyn table.
// NIL if ref_table is nil. Implemented in ui.c.
Val ui_ref_get(Val ref_table, Val ref_value);

// ---------------------------------------------------------------- UIBox (engine/ui.lua:2)
// Inherited members the Lua uses: self.config (Node.config; the args.config table itself, keys: align or type,
// bond, offset {x=, y=}, lr_clamp, major, parent, xy_bond, r_bond, wh_bond, scale_bond, can_collide,
// instance_type: a key of G.I, nil as 'UIBOX'), self.parent (Node.parent = config.parent), self.Mid (Moveable.Mid:
// the element whose config.mid is set, the last one built winning; else what Moveable_init left, the box itself),
// self.children (Node.children: 'h_popup', 'alert' and whatever a file hangs by name).
// in dyn: (NOT members) whatever another file hangs on a UIBox (attention_text...).
struct UIBox {
    Moveable moveable;
    Table *definition;   // self.definition: the root definition node {n=, config=, nodes=}; never NULL
    UIElement *UIRoot;   // self.UIRoot: the root element (its Node.parent is this box); NULL only during init
    Table *draw_layers;  // self.draw_layers: config.draw_layer (1, 2...) -> O(UIElement); never NULL; drawn after
                         // the tree in ipairs order (stops at the first gap)
    bool attention_text; // self.attention_text (nil as false): UIDEF attention_text and the boss warning set it,
                         // Game_draw reads it. A member, not a dyn field: it is read every frame.
};
// UIBox{...}: NEW + init, as an expression (definitions nest boxes inside tables).
static inline UIBox *UIBox_new(Table *args);
// ui.lua:18 args: {definition = node table, config = table (required: line 93 indexes it), T = ignored (the Lua
// passes {args.T} to Moveable.init, which then takes the defaults 0, 0, 1, 1)}. Adds itself to
// G.I[config.instance_type or 'UIBOX'].
void UIBox_init(UIBox *self, Table *args);
// ui.lua:101 id: compared with veq to config.id (ids are strings or numbers: S("x") / N(i)). node: where to start,
// a UIElement cast to Node (the walk also crosses the keyed 'h_popup' / 'alert' UIBox children); NULL: UIRoot.
// Returns NULL as nil.
UIElement *UIBox_get_UIE_by_ID(UIBox *self, Val id, Node *node);
// ui.lua:118 Lays out node and its subtree. _T: the transform whose x, y (and w, h for a T node) are read on entry
// (the box's T for the root, the parent's running local _ct for a child). recalculate: false as nil. _scale < 0:
// nil (default 1). Returns the Lua's two results in *w, *h; the function's value is false when the Lua returns a
// nil w or h (a B or O node with no size: the parent then skips it), and *w, *h are then not to be used.
// note: on a recalculate the Lua writes that nil into T.w / T.h; a Transform cannot hold it (the game never builds
// a B without w and h or an O without object).
bool UIBox_calculate_xywh(UIBox *self, UIElement *node, const Transform *_T, bool recalculate, float _scale, float *w, float *h);
// ui.lua:219 Never called by the game. node NULL: UIRoot. group: compared with veq. true if node was removed.
bool UIBox_remove_group(UIBox *self, Node *node, Val group);
// ui.lua:229 node NULL: UIRoot. group: compared with veq to config.group (NIL never matches). ingroup NULL: a new
// table. Returns ingroup: an array of O(UIElement), children before their parent.
Table *UIBox_get_group(UIBox *self, Node *node, Val group, Table *ingroup);
// ui.lua:239 Builds the UIElement of the definition node 'node' and of its subtree. parent NULL: it is the root.
void UIBox_set_parent_child(UIBox *self, Table *node, UIElement *parent);
// ui.lua:270
void UIBox_remove(UIBox *self);
// ui.lua:283
void UIBox_draw(UIBox *self);
// ui.lua:306 (callers pass true or false: the Lua ignores it)
void UIBox_recalculate(UIBox *self);
// ui.lua:320
void UIBox_move(UIBox *self, float dt);
// ui.lua:325 offset NULL: none given. (The Lua moves UIRoot with an undefined dt: nil.)
void UIBox_drag(UIBox *self, const Vec2 *offset);
// ui.lua:330 node: a definition node table. parent: the element it goes under (never NULL in the game).
void UIBox_add_child(UIBox *self, Table *node, UIElement *parent);
// ui.lua:335
void UIBox_set_container(UIBox *self, Node *container);
// ui.lua:340 Debug: nothing calls it. indent: 0 as nil.
const char *UIBox_print_topology(UIBox *self, int indent);

// ---------------------------------------------------------------- UIElement (engine/ui.lua:348)
// The cache of draw_pixellated_rect (self.pixellated_rect): flat x, y lists in the Lua's order; vertex k of the Lua
// (1-based) is [k - 1].
typedef struct UIPixellatedRect {
    float w, h, sw, sh, progress, parallax; // what it was built for: VT.w, VT.h, shadow_parrallax.x, .y, progress, _parallax
    bool has_emboss;         // built with an _emboss: emboss and line_emboss are filled (else their Lua lists are empty)
    float fill[60], shadow[60], emboss[60]; // 30 points (the first is the fan's centre)
    float line[56], line_emboss[56];        // points 3..30
} UIPixellatedRect;
// Inherited members the Lua uses: self.config, self.children, self.parent (see the top of the file), self.T / VT,
// self.states, self.role (set_values: Minor of the UIBox, offset = the layout position plus the alignment shifts),
// self.static_rotation (true), self.layered_parallax, self.shadow_parrallax, self.under_overlay: in Moveable / Node.
// in dyn: (NOT members) whatever another file hangs on an element.
// On ANY object (the config.object of an O node), in ITS dyn, never a member, also for a DynaText:
//   ui_object_updated (true or nil; DynaText_update_text and the Jimbo code set it, UIElement_update_object clears it),
//   non_recalc (bool).
struct UIElement {
    Moveable moveable;
    UIType UIT;          // self.UIT: the node type
    UIBox *UIBox;        // self.UIBox: the box it belongs to; never NULL
    struct { float w, h; } content_dimensions; // size of its content with the padding, game units ({0, 0} at init)
    bool has_last_clicked;      // self.last_clicked ~= nil
    num last_clicked;           // G.TIMERS.REAL of the last click that fired the button
    bool button_clicked;        // set by click, cleared by the next update (nil as false)
    bool disable_button;        // set by a one_press click; other files reset it (nil as false)
    bool has_focus_timer;       // self.focus_timer ~= nil
    num focus_timer;            // G.TIMERS.REAL when the focus outline started
    bool has_object_focus_timer;
    num object_focus_timer;     // the same for an O node whose object has the focus
    UIPixellatedRect *pixellated_rect; // NULL as nil; allocated by the first draw_pixellated_rect, freed by remove
    // Copies of the config keys that the per-frame code reads and that NO Lua code assigns after the element is
    // created; UIElement_init fills them from config (nil as said for each). Every other key is read from the
    // config table each time, notably: w, minh, scale, text, colour, outline_colour, outline, shadow, hover, chosen,
    // draw_after, force_focus, id, object, lang, text_drawable, prev_value, ref_table, func, button, button_UIE,
    // button_temp, button_delay (_start, _end, _progress), juice, group, h_popup, h_popup_config: files change those.
    struct {
        int draw_layer;         // 0 as nil
        bool force_collision;
        float button_dist;      // 1 as nil
        bool vert;
        const float *shadow_colour; // NULL as nil
        bool r;                 // config.r is truthy (rounded: the Lua only tests it; 0 is truthy)
        bool has_emboss;        // config.emboss ~= nil (0 counts)
        float emboss;           // game units
        bool focus_with_object;
        bool has_line_emboss;
        float line_emboss;
        float ext_up;           // game units; 0 as nil
        bool has_res;
        float res;
        float padding;          // config.padding or G.UIT.padding
        bool has_h;             // (config.w is not cached: button callbacks change it)
        float h;
        bool has_maxw, has_maxh;
        float maxw, maxh;
        float minw;             // 0 as nil (config.minh is not cached)
        bool no_recalc;
        Val ref_value;          // NIL as nil
        Table *progress_bar;    // {empty_col, filled_col, ref_table, ref_value, max}; NULL as nil
        Table *role;            // the role given to config.object instead of the default; NULL as nil
        bool no_role;
    } cfg;
    // not the Lua's: the config keys that update and draw read every frame and that files DO assign (the ones cfg
    // cannot hold), as they were when config had this stamp (core.h, tstamp): any write to config, by anyone, makes
    // the next UI_snap() read them again. Always through UI_snap(e), at every use (it is two compares): never keep
    // the pointer across a call that may write config (a G.FUNCS callback, a tset).
    struct UISnap {
        Table *config;
        uint64_t stamp;
        Val button, button_temp, button_delay, func, scale, outline, chosen, text, ref_table, prev_value, object,
            text_drawable;      // (text_drawable: P(Text *) or NIL)
        UIElement *button_UIE;      // NULL as nil
        const float *colour, *outline_colour; // NULL as nil
        Table *lang;                // NULL as nil
        bool shadow, hover, force_focus, draw_after; // truthy
    } snap;
    // not the Lua's, for update_text and update_object: the bound table (ref_table when it is a table) and the
    // object's dyn, with the stamps they had when this element last found nothing to do there
    Table *idle_ref; uint64_t idle_ref_stamp, idle_cfg_stamp;
    Table *idle_dyn; uint64_t idle_dyn_stamp;
};
void UI_snap_fill(UIElement *e);
// A font table's numbers (font.FONTSCALE, .squish, .TEXT_OFFSET.x / .y as floats), read again only when the table or
// its TEXT_OFFSET was written (their stamps). One font at a time is kept: the game draws with one.
struct FontNums { Table *font, *offs; uint64_t stamp, ostamp; float FONTSCALE, squish, off_x, off_y; };
const struct FontNums *font_nums(Table *font);
// config.lang.font of a text element / G.LANG: lang's "font", read again only when lang was written
Table *lang_font(Table *lang);
static inline const struct UISnap *UI_snap(UIElement *e) {
    Table *config = e->moveable.node.config;
    if (e->snap.config != config || e->snap.stamp != tstamp(config)) UI_snap_fill(e);
    return &e->snap;
}
// ui.lua:350 parent: the parent UIElement cast to Node, NULL as nil (the root: set_parent_child then sets the box).
// config NULL: a new empty table. Does not call Moveable_init (set_values does): T is NULL after it.
void UIElement_init(UIElement *self, Node *parent, UIBox *new_UIBox, UIType new_UIT, Table *config);
// ui.lua:360 _T: only x, y, w, h are read. recalculate: false as nil. Runs Moveable_init again unless recalculate
// and self.T exists.
void UIElement_set_values(UIElement *self, const Transform *_T, bool recalculate);
// ui.lua:466 Debug: only UIBox_print_topology calls it.
const char *UIElement_print_topology(UIElement *self, int indent);
// ui.lua:495 (the root's caller passes true: the Lua ignores it)
void UIElement_initialize_VT(UIElement *self);
// ui.lua:519 amount < 0: nil; rot_amt NIL: nil (both passed on unchanged to the object's or Moveable's juice_up).
void UIElement_juice_up(UIElement *self, float amount, Val rot_amt);
// ui.lua:527 The Node to drag, or NULL.
Node *UIElement_can_drag(UIElement *self);
// ui.lua:532 Empty.
void UIElement_draw(UIElement *self);
// ui.lua:535 (the Lua's 'layer' argument is never passed nor read)
void UIElement_draw_children(UIElement *self);
// ui.lua:547 Returns T.w and, in *h, T.h.
float UIElement_set_wh(UIElement *self, float *h);
// ui.lua:576
void UIElement_align(UIElement *self, float x, float y);
// ui.lua:586
void UIElement_set_alignments(UIElement *self);
// ui.lua:617
void UIElement_update_text(UIElement *self);
// ui.lua:631
void UIElement_update_object(UIElement *self);
// ui.lua:663
void UIElement_draw_self(UIElement *self);
// ui.lua:853 _type: 'fill', 'shadow', 'line', 'emboss' or 'line_emboss'. _emboss NIL: nil (N(0) is given and
// counts). _progress NIL: nil (default 1).
void UIElement_draw_pixellated_rect(UIElement *self, const char *_type, float _parallax, Val _emboss, Val _progress);
// ui.lua:938
void UIElement_update(UIElement *self, float dt);
// ui.lua:957
bool UIElement_collides_with_point(UIElement *self, Vec2 cursor_trans);
// ui.lua:965
void UIElement_click(UIElement *self);
// ui.lua:998 A 'tab' element with no chosen child returns nothing in the Lua: *x, *y are then left untouched.
void UIElement_put_focused_cursor(UIElement *self, float *x, float *y);
// ui.lua:1008
void UIElement_remove(UIElement *self);
// ui.lua:1022
void UIElement_hover(UIElement *self);
// ui.lua:1038
void UIElement_stop_hover(UIElement *self);
// ui.lua:1045 other: the dragged Node, NULL as nil.
void UIElement_release(UIElement *self, void *other);
// ui.lua:1049 (the Lua's spelling)
bool is_UI_containter(UIElement *node);

// ---------------------------------------------------------------- DynaText (engine/text.lua:2)
// A letter of a string: strings[k].letters[i]
typedef struct DynaLetter {
    Text *letter;        // love.graphics.newText(font.FONT, char): Text_release it when the letters are rebuilt
    const char *chr;     // .char: the UTF-8 character
    float scale;         // part scale at creation; 1 + pulse + quiver after every align_letters
    Vec2 offset;         // font pixels; y animated by float / bump, x never written ({0, 0}; copied from the old
                         // letter at the same index when rebuilt)
    Vec2 dims;           // font pixels
    float pop_in;        // 0..1
    float r;             // rotation; 0 as nil (before the first align_letters)
    const float *prefix, *suffix, *colour; // the part's outer_colour (prefix / suffix letters) and colour; NULL as nil
} DynaLetter;
// self.strings[k]
typedef struct DynaString {
    const char *string;  // NULL as nil (a new entry)
    DynaLetter *letters; // the Lua's letters[i] is letters[i - 1]
    int nletters;        // #letters
    float W, H;          // game units
    float W_offset, H_offset;
    // not the Lua's: the last prefix..tostring(value)..suffix that DynaText_update_text built for this part and
    // what it was built from (see there); memo_string NULL: none yet
    Val memo_value;
    const char *memo_prefix, *memo_suffix, *memo_string;
    // not the Lua's: the part's table and the table it is bound to (ref_table, when it is a table read by a string
    // key), with the stamps (core.h, tstamp) they had when update_text last left this string up to date; NULL: none
    Table *idle_part, *idle_ref;
    uint64_t idle_part_stamp, idle_ref_stamp;
} DynaString;
// Inherited members the Lua uses: self.config (Node.config: the constructor's table itself), self.children
// ('particle_effect': a Particles, by name), self.T.r (config.text_rot or 0), self.shadow_parrallax.
// The config stays a Table and is read each time for the keys the Lua changes: string (init wraps a lone value into
// {value}: an array of S(text) or T({ref_table=, ref_value=} or {string=}, with prefix, suffix, scale, colour,
// outer_colour)), W, H, pop_in, pop_out (nil-ness is state: N(0) is truthy), pulse {speed, width, start, amount,
// silent}, quiver {speed, amount, silent}. Read once by init only: scale, pop_in_rate, bump_rate, bump_amount, font,
// colours, shadow, silent, x_offset, y_offset, maxw, X, Y, text_rot.
// in dyn: (NOT members) ui_object_updated, non_recalc (see UIElement: the element reads them on any object).
struct DynaText {
    Moveable moveable;
    // not the Lua's: the config keys update reads every frame, as they were at this stamp of config (core.h, tstamp),
    // and the W, H last written to config at wh_stamp. text.c only.
    struct { Table *config; uint64_t stamp; Table *string, *pulse, *quiver; Val pop_out, pop_in; } cfg_snap;
    uint64_t wh_stamp; num wh_W, wh_H;
    bool shadow;         // config.shadow
    float scale;         // config.scale or 1, shrunk once by init to fit config.maxw; button callbacks assign it
    float pop_in_rate;   // 3 by default
    float bump_rate;     // 2.666 by default
    float bump_amount;   // 1 by default
    Table *font;         // config.font or G.LANG.font: the font table {FONT, FONTSCALE, TEXT_OFFSET, ...}; never NULL
    const char *string;  // the focused string, set by align_letters; NULL before (the Lua then holds the raw
                         // config.string[1], which nothing reads)
    Vec2 text_offset;    // font.TEXT_OFFSET * scale + config.x_offset / y_offset, with the scale before the maxw shrink
    Table *colours;      // config.colours or {G.C.RED}: array of P(float *); never NULL
    num created_time;    // G.TIMERS.REAL when the pop-in (re)started
    bool silent;         // no pop-in sound
    bool start_pop_in;   // truthiness of the Lua's value (config.pop_in at init, N(0) counts; false after update_text)
    bool reset_pop_in;
    DynaString *strings; // self.strings[k] is strings[k - 1]
    int nstrings;        // #self.strings
    int focused_string;  // 1-based: use DynaText_focused
    float pop_delay;     // seconds before the pop-out; 0 as nil (other files set 0)
    num pop_out_time;    // G.TIMERS.REAL at which the pop-out begins (set by pop_out before any read)
    bool pop_cycle;      // move to the next string at the next align_letters (nil as false)
    // Copies of the config keys that the per-frame code reads and that no Lua code assigns after creation;
    // DynaText_init fills them at its start.
    struct {
        float spacing;          // 0 as nil (equivalent: the Lua's only truthiness test translates by spacing)
        float min_cycle_time;   // 1 as nil; 0 means always fully shown (the Lua's '== 0' tests)
        const float *shadow_colour; // NULL as nil
        bool reset_pop_in;
        bool non_recalc;
        float offset_y;         // 0 as nil
        bool random_element;
        float pitch_shift;      // 0 as nil
        float pop_delay;        // config.pop_delay or 1.5
        int rotate;             // 0: nil or false; 2: config.rotate == 2; 1: any other truthy value
        bool float_;            // config.float
        bool bump;
        bool has_maxw;
        float maxw;
    } cfg;
};
// self.strings[self.focused_string]
static inline DynaString *DynaText_focused(DynaText *self) { return &self->strings[self->focused_string - 1]; }
// DynaText({...}): NEW + init, as an expression (definitions build them inside tables).
static inline DynaText *DynaText_new(Table *config);
// text.lua:5 config NULL: {} (which the Lua cannot survive: config.string is required). Adds itself to G.I.MOVEABLE.
void DynaText_init(DynaText *self, Table *config);
// text.lua:64
void DynaText_update(DynaText *self, float dt);
// text.lua:69 first_pass: false as nil.
void DynaText_update_text(DynaText *self, bool first_pass);
// text.lua:156 pop_out_timer < 0: nil (default 1).
void DynaText_pop_out(DynaText *self, float pop_out_timer);
// text.lua:161 pop_in_timer < 0: nil (default 0).
void DynaText_pop_in(DynaText *self, float pop_in_timer);
// text.lua:174
void DynaText_align_letters(DynaText *self);
// text.lua:239 amt < 0: nil (default 0.7).
void DynaText_set_quiver(DynaText *self, float amt);
// text.lua:247 amt < 0: nil (default 0.2).
void DynaText_pulse(DynaText *self, float amt);
// text.lua:257
void DynaText_draw(DynaText *self);

static inline UIBox *UIBox_new(Table *args) {
    UIBox *self = NEW(UIBox);
    UIBox_init(self, args);
    return self;
}
static inline DynaText *DynaText_new(Table *config) {
    DynaText *self = NEW(DynaText);
    DynaText_init(self, config);
    return self;
}
#endif
