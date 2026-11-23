// engine/node.lua:1-389: the Node class, the base of everything that has a transform.
#include "all.h"

// The Class slots take void *self: thin wrappers with the slots' own signatures.
static void cls_update(void *self, float dt) { Node_update(self, dt); }
static void cls_draw(void *self) { Node_draw(self); }
static void cls_remove(void *self) { Node_remove(self); }
static void cls_hover(void *self) { Node_hover(self); }
static void cls_stop_hover(void *self) { Node_stop_hover(self); }
static void cls_click(void *self) { Node_click(self); }
static void cls_release(void *self, void *dragged) { Node_release(self, dragged); }
static void cls_drag(void *self, const Vec2 *offset) { Node_drag(self); } // Node:drag() takes no offset
static void cls_stop_drag(void *self) { Node_stop_drag(self); }
static void *cls_can_drag(void *self) { return Node_can_drag(self); }
static void cls_animate(void *self, float dt) { Node_animate(self, dt); }
static bool cls_collides_with_point(void *self, Vec2 point) { return Node_collides_with_point(self, point); }
static void cls_set_container(void *self, Node *container) { Node_set_container(self, container); }
static void cls_put_focused_cursor(void *self, float *x, float *y) { Node_put_focused_cursor(self, x, y); }

// node.lua:2 Node = Object:extend()
Class Node_class = {
    .name = "Node",
    .super = &Object_class,
    .update = cls_update,
    .draw = cls_draw,
    .remove = cls_remove,
    .hover = cls_hover,
    .stop_hover = cls_stop_hover,
    .click = cls_click,
    .release = cls_release,
    .drag = cls_drag,
    .stop_drag = cls_stop_drag,
    .can_drag = cls_can_drag,
    .animate = cls_animate,
    .collides_with_point = cls_collides_with_point,
    .set_container = cls_set_container,
    .put_focused_cursor = cls_put_focused_cursor,
};

// args.T.<name> or args.T[i] or def (0 is truthy: an explicit 0 stays 0)
static float T_arg(Table *argsT, const char *name, int i, float def) {
    Val v = tget(argsT, name);
    if (!truthy(v)) v = tgeti(argsT, i);
    return truthy(v) ? (float)vnum(v) : def;
}

// node.lua:11
void Node_init(Node *self, Table *args) {
    // From args, set the values of self transform (the Lua writes the defaults into the caller's table too)
    if (!args) args = tnew();
    if (!truthy(tget(args, "T"))) tsett(args, "T", tnew());
    Table *argsT = ttab(args, "T");

    // self.ARGS / self.RETS: scratch tables, they do not exist here (node.h)

    // Config table used for any metadata about this node
    if (!self->config) self->config = tnew();

    // For transform init, accept params in the form x|1, y|2, w|3, h|4, r|5
    self->T_own = (Transform){
        .x = T_arg(argsT, "x", 1, 0),
        .y = T_arg(argsT, "y", 2, 0),
        .w = T_arg(argsT, "w", 3, 1),
        .h = T_arg(argsT, "h", 4, 1),
        .r = T_arg(argsT, "r", 5, 0),
        .scale = T_arg(argsT, "scale", 6, 1),
    };
    self->T = &self->T_own;
    // Transform to use for collision detection
    self->CT = self->T;

    // Create the offset tables, used to determine things like drag offset and 3d shader effects
    self->click_offset = (Vec2){0, 0};
    self->hover_offset = (Vec2){0, 0};

    // If true, this node moves normally even when the G.TIMERS.TOTAL doesn't increment
    // note: the member is a bool, so the nil of G.SETTINGS.paused at boot is false here
    self->created_on_pause = tbool(G->SETTINGS, "paused");

    // ID tracker, every Node has a unique ID
    if (!G->ID) G->ID = 1; // G.ID = G.ID or 1 (0 as nil)
    self->ID = G->ID;
    G->ID = G->ID + 1;

    // Frame tracker to aid in not doing too many extra calculations
    self->FRAME.DRAW = -1;
    self->FRAME.MOVE = -1;

    // All nodes do not collide by default
    self->states = (States){
        .visible = true,
        .collide = {.can = false, .is = false},
        .focus = {.can = false, .is = false},
        .hover = {.can = true, .is = false},
        .click = {.can = true, .is = false},
        .drag = {.can = true, .is = false},
        .release_on = {.can = true, .is = false},
    };

    // If we provide a container, all nodes within that container are translated with it as the reference frame
    Node *container = tobj(args, "container");
    self->container = container ? container : G->ROOM;

    // The list of children give Node a treelike structure
    if (!self->children) {
        self->children = tnew();
    }

    // Add this object to the instance table only if the metatable matches with NODE
    if (self->object.cls == &Node_class) {
        tinsert(G->I.NODE, O(self));
    }

    // Unless created during a stage transition, add the node to its stage object table
    if (!G->STAGE_OBJECT_INTERRUPT) {
        tinsert(ttabi(G->STAGE_OBJECTS, G->STAGE), O(self));
    }
}

// node.lua:91
void Node_draw_boundingrect(Node *self) {
    self->under_overlay = G->under_overlay;

    // node.lua:94-123: debug only (G.DEBUG), not translated
}

// node.lua:127
void Node_draw(Node *self) {
    Node_draw_boundingrect(self);
    if (self->states.visible) {
        add_to_drawhash(self);
        // note: pairs(self.children): the array part, then the named keys in insertion order
        for (TIter it = titer(self->children); tnext(&it);) {
            CALL(it.val.obj, draw);
        }
    }
}

// node.lua:143 (true, or false as the Lua's nil)
bool Node_collides_with_point(Node *self, Vec2 point) {
    // First reset the collision state to false
    if (self->container) {
        Transform *T = self->CT ? self->CT : self->T;
        Vec2 _p, _t;
        float _r_cos, _r_sin;

        float _b = self->states.hover.is ? G->COLLISION_BUFFER : 0;

        _p.x = point.x, _p.y = point.y;

        if (self->container != self) { // apply all translations and rotations for the container first
            if (fabsf(self->container->T->r) < 0.1f) {
                // Translate to normalize this Node to the center of the container
                _t.x = -self->container->T->w / 2, _t.y = -self->container->T->h / 2;
                point_translate(&_p, _t);

                // Rotate node about the center of the container
                point_rotate(&_p, self->container->T->r);

                // Translate node to undo the container translation, reframing it in 'container' space
                _t.x = self->container->T->w / 2 - self->container->T->x, _t.y = self->container->T->h / 2 - self->container->T->y;
                point_translate(&_p, _t);
            } else {
                // Translate node to undo the container translation, reframing it in 'container' space
                _t.x = -self->container->T->x, _t.y = -self->container->T->y;
                point_translate(&_p, _t);
            }
        }
        if (fabsf(T->r) < 0.1f) {
            // If we can essentially disregard transform rotation, just treat it like a normal rectangle
            if (_p.x >= T->x - _b && _p.y >= T->y - _b && _p.x <= T->x + T->w + _b && _p.y <= T->y + T->h + _b) {
                return true;
            }
        } else {
            // Otherwise rotate the point to determine if it intersects the rotated rectangle
            _r_cos = cosf(T->r + (float)M_PI / 2), _r_sin = sinf(T->r + (float)M_PI / 2);
            _p.x = _p.x - (T->x + 0.5f * (T->w)), _p.y = _p.y - (T->y + 0.5f * (T->h));
            _t.x = _p.y * _r_cos - _p.x * _r_sin, _t.y = _p.y * _r_sin + _p.x * _r_cos;
            _p.x = _t.x + (T->x + 0.5f * (T->w)), _p.y = _t.y + (T->y + 0.5f * (T->h));

            if (_p.x >= T->x - _b && _p.y >= T->y - _b
                && _p.x <= T->x + T->w + _b && _p.y <= T->y + T->h + _b) {
                return true;
            }
        }
    }
    return false;
}

// node.lua:202
void Node_set_offset(Node *self, Vec2 point, const char *type) {
    Vec2 _p, _t;

    _p.x = point.x, _p.y = point.y;

    // Translate to middle of the container
    _t.x = -self->container->T->w / 2;
    _t.y = -self->container->T->h / 2;
    point_translate(&_p, _t);

    // Rotate about the container midpoint according to node rotation
    point_rotate(&_p, self->container->T->r);

    // Translate node to undo the container translation, reframing it in 'container' space
    _t.x = self->container->T->w / 2 - self->container->T->x;
    _t.y = self->container->T->h / 2 - self->container->T->y;
    point_translate(&_p, _t);

    if (streq(type, "Click")) {
        self->click_offset.x = (_p.x - self->T->x);
        self->click_offset.y = (_p.y - self->T->y);
    } else if (streq(type, "Hover")) {
        self->hover_offset.x = (_p.x - self->T->x);
        self->hover_offset.y = (_p.y - self->T->y);
    }
}

// node.lua:233
void Node_drag(Node *self) {
    if (self->config && truthy(tget(self->config, "d_popup"))) {
        if (!truthy(tget(self->children, "d_popup"))) {
            tseto(self->children, "d_popup", UIBox_new(TAB(
                "definition", tget(self->config, "d_popup"),
                "config", tget(self->config, "d_popup_config"))));
            // note: line 240 indexes h_popup, not d_popup (the Lua's own slip): it is an error with no h_popup
            Node *h_popup = tobj(self->children, "h_popup");
            if (!h_popup) ERROR("node.lua:240: attempt to index field 'h_popup' (a nil value)");
            h_popup->states.collide.can = false;
            tinsert(G->I.POPUP, tget(self->children, "d_popup"));
            ((Node *)tobj(self->children, "d_popup"))->states.drag.can = true;
        }
    }
}

// node.lua:249
Node *Node_can_drag(Node *self) {
    return self->states.drag.can ? self : NULL;
}

// node.lua:254
void Node_stop_drag(Node *self) {
    if (truthy(tget(self->children, "d_popup"))) {
        // note: pairs with table.remove and no break: the walk goes on at k + 1 over the shifted array
        for (int k = 1; k <= tlen(G->I.POPUP); k++) {
            if (veq(tgeti(G->I.POPUP, k), tget(self->children, "d_popup"))) {
                tremove(G->I.POPUP, k);
            }
        }
        CALL(tobj(self->children, "d_popup"), remove);
        tset(self->children, "d_popup", NIL);
    }
}

// node.lua:267
void Node_hover(Node *self) {
    if (self->config && truthy(tget(self->config, "h_popup"))) {
        if (!truthy(tget(self->children, "h_popup"))) {
            Table *h_popup_config = ttab(self->config, "h_popup_config");
            if (!h_popup_config) ERROR("node.lua:270: attempt to index field 'h_popup_config' (a nil value)");
            tsets(h_popup_config, "instance_type", "POPUP");
            tseto(self->children, "h_popup", UIBox_new(TAB(
                "definition", tget(self->config, "h_popup"),
                "config", tget(self->config, "h_popup_config"))));
            Node *h_popup = tobj(self->children, "h_popup");
            h_popup->states.collide.can = false;
            h_popup->states.drag.can = true;
        }
    }
}

// node.lua:282
void Node_stop_hover(Node *self) {
    if (truthy(tget(self->children, "h_popup"))) {
        CALL(tobj(self->children, "h_popup"), remove);
        tset(self->children, "h_popup", NIL);
    }
}

// node.lua:290 (the two results through x and y)
void Node_put_focused_cursor(Node *self, float *x, float *y) {
    *x = (self->T->x + self->T->w / 2 + self->container->T->x) * (G->TILESCALE * G->TILESIZE);
    *y = (self->T->y + self->T->h / 2 + self->container->T->y) * (G->TILESCALE * G->TILESIZE);
}

// node.lua:297
void Node_set_container(Node *self, Node *container) {
    if (self->children) {
        for (TIter it = titer(self->children); tnext(&it);) {
            CALL(it.val.obj, set_container, container);
        }
    }
    self->container = container;
}

// node.lua:307
void Node_translate_container(Node *self) {
    if (self->container && self->container != self) {
        love_graphics_translate(self->container->T->w * G->TILESCALE * G->TILESIZE * 0.5f, self->container->T->h * G->TILESCALE * G->TILESIZE * 0.5f);
        love_graphics_rotate(self->container->T->r);
        love_graphics_translate(
            -self->container->T->w * G->TILESCALE * G->TILESIZE * 0.5f + self->container->T->x * G->TILESCALE * G->TILESIZE,
            -self->container->T->h * G->TILESCALE * G->TILESIZE * 0.5f + self->container->T->y * G->TILESCALE * G->TILESIZE);
    }
}

// for k, v in pairs(list) do if v == self then table.remove(list, k) break end end
static void remove_first(Table *list, Node *self) {
    for (int k = 1; k <= tlen(list); k++) {
        Val v = tgeti(list, k);
        if (v.t == V_OBJ && v.obj == (void *)self) {
            tremove(list, k);
            break;
        }
    }
}

// node.lua:319
void Node_remove(Node *self) {
    remove_first(G->I.POPUP, self);
    remove_first(G->I.NODE, self);
    remove_first(ttabi(G->STAGE_OBJECTS, G->STAGE), self);
    if (self->children) {
        for (TIter it = titer(self->children); tnext(&it);) {
            CALL(it.val.obj, remove);
        }
    }
    if (G->CONTROLLER->clicked.target == self) {
        G->CONTROLLER->clicked.target = NULL;
    }
    if (G->CONTROLLER->focused.target == self) {
        G->CONTROLLER->focused.target = NULL;
    }
    if (G->CONTROLLER->dragging.target == self) {
        G->CONTROLLER->dragging.target = NULL;
    }
    if (G->CONTROLLER->hovering.target == self) {
        G->CONTROLLER->hovering.target = NULL;
    }
    if (G->CONTROLLER->released_on.target == self) {
        G->CONTROLLER->released_on.target = NULL;
    }
    if (G->CONTROLLER->cursor_down.target == self) {
        G->CONTROLLER->cursor_down.target = NULL;
    }
    if (G->CONTROLLER->cursor_up.target == self) {
        G->CONTROLLER->cursor_up.target = NULL;
    }
    if (G->CONTROLLER->cursor_hover.target == self) {
        G->CONTROLLER->cursor_hover.target = NULL;
    }

    self->REMOVED = true;
}

// node.lua:375 (as written: the square root covers only the x term; no caller in the game)
float Node_fast_mid_dist(Node *self, Node *other_node) {
    return powf(sqrtf((other_node->T->x + 0.5f * other_node->T->w) - (self->T->x + self->T->w)), 2) + powf((other_node->T->y + 0.5f * other_node->T->h) - (self->T->y + self->T->h), 2);
}

// node.lua:380 Prototype for a click release function, when the cursor is released on this node
void Node_release(Node *self, void *dragged) {}

// node.lua:383 Prototype for a click function
void Node_click(Node *self) {}

// node.lua:386 Prototype animation function for any frame manipulation needed
void Node_animate(Node *self, float dt) {}

// node.lua:389 Prototype update function for any object specific logic that needs to occur every frame
void Node_update(Node *self, float dt) {}
