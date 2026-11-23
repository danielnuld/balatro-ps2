// engine/controller.lua: the Controller, all the logic of how the player's input reaches the game's objects
// (cursor, focus, hover, click, drag, the button registry). One instance: G.CONTROLLER (game.lua:140).
// Rule (docs/conventions.md): a field is a struct member iff it is declared here; any other field of the Lua object is
// in obj.dyn (self->object.dyn).
//
// How the Lua's shapes are kept:
// - Buttons, keys, HID types and directions stay the Lua's strings ("a", "dpleft", "triggerleft", "button", "L"...):
//   compare with streq. '' is "" (never NULL) where the Lua uses '' (axis_buttons, HID.last_type, GAMEPAD_CONSOLE).
// - The maps keyed by a name built at run time stay Table * with the Lua's keys (pressed_buttons, locks,
//   button_registry...): G.CONTROLLER.locks.use = true is tsetb(G->CONTROLLER->locks, "use", true).
// - The records of fixed shape are structs ("a node" is any Node subclass: cast to Node *, test with is_a).
// - The scratch tables G.ARGS.focus_list, G.ARGS.focusables, G.ARGS.focus_cursor_pos, G.ARGS.focus_vec,
//   G.ARGS.gamepad_patterns and the accidental global _ft (controller.lua:1183) are locals of controller.c.
#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "types.h"
#include "node.h"

typedef struct Joystick Joystick; // LOVE's joystick object (the platform's); used only through pointers

// self.clicked, self.focused, self.dragging, self.hovering, self.released_on
typedef struct {
    Node *target;      // the node this frame; NULL as nil
    bool handled;      // init true
    Node *prev_target; // last frame's target; NULL as nil
} ControllerTarget;

// self.cursor_down, self.cursor_up, self.cursor_hover
typedef struct {
    Vec2 T;            // where it happened, in game units (the Lua replaces the table on each press: assign the struct)
    Node *target;      // NULL as nil
    num time;          // G.TIMERS.TOTAL when it happened (init 0; cursor_up: 0.1). misc_functions.lua:238 seeds with cursor_hover.time
    bool handled;      // init true
    Node *prev_target; // cursor_hover only (set_cursor_hover, :989); NULL as nil
} ControllerCursor;

// an entry of self.axis_buttons
typedef struct {
    const char *current;  // the button the axis counts as now: "", "dpleft", "dpright", "dpup", "dpdown", "triggerleft", "triggerright"
    const char *previous; // last frame's
} AxisButton;

struct Controller {
    Object object;
    ControllerTarget clicked;     // the node clicked this frame (target is never reset by update, only handled)
    ControllerTarget focused;     // the node focused this frame (only with a gamepad)
    ControllerTarget dragging;    // the node being dragged
    ControllerTarget hovering;    // the node being hovered
    ControllerTarget released_on; // the node the cursor was released on after a drag
    Table *collision_list;        // array of O(Node): the nodes under the cursor with states.collide.can, top-most first; never NULL
    Table *nodes_at_cursor;       // array of O(Node): every node under the cursor (the Lua makes it on first use: here made by init); never NULL
    ControllerCursor cursor_down, cursor_up, cursor_hover;
    Node *cursor_collider;        // nil in init and never used
    Vec2 cursor_position;         // the cursor in PIXELS, not game units (game units = pixels / (G.TILESCALE*G.TILESIZE))

    // keys, straight from LOVE's callbacks: name -> true (absent as nil); never NULL
    Table *pressed_keys, *held_keys, *released_keys;
    Table *held_key_times;        // name -> seconds held (real dt); absent as nil, which is not 0
    // buttons, same shapes; never NULL
    Table *pressed_buttons, *held_buttons, *released_buttons;
    Table *held_button_times;     // name -> seconds held (real dt); absent (nil: the press was not accepted, or released) is not 0

    struct {
        bool focus; // true hides focus and hover until the next input (nil as false: button_callbacks.lua:2301 sets nil)
        bool stack; // the interrupt of the cursor context popped last (set in update, :278); nil as false
    } interrupt;

    Table *locks;   // name -> true/false/nil: "wipe", "frame", "frame_set", "use", "seal", "edition", "selling_card",
                    // "toggle_shop", "skip_blind", "boss_reroll", "shop_reroll", "no_space", "load"; and a Tag's ID as
                    // a NUMBER key (tag.lua:132: tsetv(locks, N(tag->ID), ...)); never NULL. Game:prep_stage clears it.
    bool locked;    // any lock truthy; recomputed by update each frame (nil before the first update: false)

    struct { AxisButton l_stick, r_stick, l_trig, r_trig; } axis_buttons; // handle_axis_buttons walks them in this order
    float axis_cursor_speed;      // game units per second at full tilt: 20

    Table *button_registry;       // button name -> array of {node = O(Node), menu = bool, click = true/nil}, [1] the newest; never NULL

    // self.snap_cursor_to: where the cursor goes at the next update; nil when !set
    struct {
        bool set;
        Node *node;       // NULL as nil/false
        bool has_T;       // T is not nil
        Vec2 T;           // game units
        const char *type; // "node" or "transform"
    } snap_cursor_to;

    struct {
        int layer;    // 1-based; mod_cursor_context_layer(-1) does not clamp it
        Table *stack; // sparse array [layer] -> {node = O(Node)/nil, cursor_pos = {x=, y=}, interrupt = bool}; never NULL
    } cursor_context;
    Table *cardarea_context;      // name of a cardarea global ("hand", "jokers", "shop_jokers"...) -> rank (number); never NULL

    struct {
        const char *last_type; // "", "mouse", "touch", "button", "axis", "axis_cursor"
        bool dpad, pointer, touch, controller, mouse, axis_cursor; // init: pointer and mouse true, the rest false
    } HID;

    struct {
        Joystick *object;    // the gamepad used last; NULL as nil
        const char *mapping; // its SDL mapping string; NULL as nil
        const char *name;    // NULL as nil
    } GAMEPAD;               // (GAMEPAD.temp_console is a local of set_gamepad)
    const char *GAMEPAD_CONSOLE;   // "", "Xbox", "Playstation", "Nintendo": which button glyphs
    Joystick *keyboard_controller; // the fake gamepad of the debug keybinds (main.lua:139, not _RELEASE_MODE): NULL, not ported

    bool is_cursor_down;

    // ---- fields the Lua creates outside init, used every frame
    float overlay_timer;       // seconds an overlay menu has been up (real dt); nil as 0 (the Lua reads it "or 0")
    bool frame_buttonpress;    // an input was already accepted this frame; nil as false
    struct { bool set; Val x, y; } L_cursor_queue; // queue_L_cursor_press's {x, y} in pixels (NIL as nil); nil when !set
    bool COYOTE_FOCUS;         // true/nil (capture_focused_input :1277, cleared in update)
    float repress_timer;       // the dpad's repeat delay, 0.3 then 0.1; 0 as nil ("or 0.3")
    bool no_holdcap;           // true/nil: the focused slider takes the held dpad (capture_focused_input)
    UIElement *text_input_hook; // the text input being typed into (button_callbacks.lua, ui.lua:1014); NULL as nil
    bool capslock;             // nil as false
    UIBox *screen_keyboard;    // the on-screen keyboard (button_callbacks.lua:921); NULL as nil
};
// in dyn (NOT members): last_touch_time (main.lua:184), using_touch (read only: blind.lua:429, tag.lua:514,
// UI_definitions.lua:4004, :5835; never set, so always nil), lock_input (written only: game.lua:1340, :1671, :1814,
// :3289), focus_cursor_stack and focus_cursor_stack_level (written only: game.lua:1184).

extern Class Controller_class; // extends Object; none of Class's virtual methods are used through it

// controller.lua:5
void Controller_init(Controller *self);
// controller.lua:96. _gamepad is never nil (the Lua would fail).
void Controller_set_gamepad(Controller *self, Joystick *_gamepad);
// controller.lua:116. _gamepad: the pad's NAME (GAMEPAD.name), never nil. Returns "Playstation", "Nintendo" or "Xbox".
const char *Controller_get_console_from_gamepad(Controller *self, const char *_gamepad);
// controller.lua:136. HID_type: "axis", "button", "mouse", "touch", "axis_cursor", or NULL as nil (does only the tail).
// button: unused by the function; NULL as nil.
void Controller_set_HID_flags(Controller *self, const char *HID_type, const char *button);
// controller.lua:166
void Controller_set_cursor_position(Controller *self);
// controller.lua:186. dt: the real (unscaled) frame time, seconds.
void Controller_update(Controller *self, float dt);
// controller.lua:429
void Controller_cull_registry(Controller *self);
// controller.lua:443. registry: the button's name, never nil.
void Controller_add_to_registry(Controller *self, Node *node, const char *registry);
// controller.lua:453
void Controller_process_registry(Controller *self);
// controller.lua:472. delta: 1 adds a layer, -1 removes one, -1000 all but the base, -2000 all; anything else (0:
// button_callbacks.lua:1335) only navigates the focus.
void Controller_mod_cursor_context_layer(Controller *self, int delta);
// controller.lua:500. args: {node = O(Node) (nil/false: O(NULL)), T = {x=, y=} or nil}; never NULL.
// G.CONTROLLER:snap_to{node = x} is Controller_snap_to(G->CONTROLLER, TAB("node", O(x))).
void Controller_snap_to(Controller *self, Table *args);
// controller.lua:505. _cardarea: the NAME of a cardarea global of G ("hand", "jokers", "consumeables", "shop_jokers").
// Returns true where the Lua returns true, false where it returns nothing.
bool Controller_save_cardarea_focus(Controller *self, const char *_cardarea);
// controller.lua:517. _cardarea: S(name) (a cardarea global of G: "hand", "pack_cards", "jokers"...) or O(CardArea *)
// (button_callbacks.lua:2305); NIL as nil.
void Controller_recall_cardarea_focus(Controller *self, Val _cardarea);
// controller.lua:541. hard_set_T: game units; NULL as nil (then the cursor goes onto focused.target, if any).
void Controller_update_cursor(Controller *self, const Vec2 *hard_set_T);
// controller.lua:561. (The Lua uses G.CONTROLLER, not self.)
void Controller_handle_axis_buttons(Controller *self);
// controller.lua:575. Returns "axis_cursor", "button", or NULL as nil.
const char *Controller_update_axis(Controller *self, float dt);
// controller.lua:690
void Controller_button_press_update(Controller *self, const char *button, float dt);
// controller.lua:742
void Controller_button_hold_update(Controller *self, const char *button, float dt);
// controller.lua:759. dt unused.
void Controller_button_release_update(Controller *self, const char *button, float dt);
// controller.lua:768. dt unused: 0 where the Lua passes none (button_callbacks.lua:889).
void Controller_key_press_update(Controller *self, const char *key, float dt);
// controller.lua:893
void Controller_key_hold_update(Controller *self, const char *key, float dt);
// controller.lua:922. dt unused.
void Controller_key_release_update(Controller *self, const char *key, float dt);
// controller.lua:934
void Controller_key_press(Controller *self, const char *key);
// controller.lua:939
void Controller_key_release(Controller *self, const char *key);
// controller.lua:944
void Controller_button_press(Controller *self, const char *button);
// controller.lua:949
void Controller_button_release(Controller *self, const char *button);
// controller.lua:954. cursor_trans: game units (G.CURSOR.T's x and y).
void Controller_get_cursor_collision(Controller *self, Vec2 cursor_trans);
// controller.lua:984
void Controller_set_cursor_hover(Controller *self);
// controller.lua:1018. x, y: pixels; NIL as nil (kept as given in L_cursor_queue).
void Controller_queue_L_cursor_press(Controller *self, Val x, Val y);
// controller.lua:1026. x, y unused: NIL as nil (controller.lua:734 passes none).
void Controller_queue_R_cursor_press(Controller *self, Val x, Val y);
// controller.lua:1037. x, y: pixels; NIL as nil: cursor_position.x / .y.
void Controller_L_cursor_press(Controller *self, Val x, Val y);
// controller.lua:1060. x, y: pixels; NIL as nil: cursor_position.x / .y.
void Controller_L_cursor_release(Controller *self, Val x, Val y);
// controller.lua:1079. node: never nil.
bool Controller_is_node_focusable(Controller *self, Node *node);
// controller.lua:1112. dir: "U", "D", "L", "R", or NULL as nil (focus what the cursor is on).
void Controller_update_focus(Controller *self, const char *dir);
// controller.lua:1262. input_type: "press" or "hold". Returns true only where the Lua returns exactly true (false and
// the nil of :1327/:1336 are both false: the one caller that reads it only tests "not").
bool Controller_capture_focused_input(Controller *self, const char *button, const char *input_type, float dt);
// controller.lua:1375. dir: as update_focus; NULL as nil.
void Controller_navigate_focus(Controller *self, const char *dir);
#endif
