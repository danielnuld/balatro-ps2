// engine/object.lua: the base of the class system.
#include "all.h"
#include "needs/object.h"

// object.lua:4 Object = {}
Class Object_class = {.name = "Object", .super = NULL};

// object.lua:6 Object:init() is empty and node.h declares no prototype for it: nothing to call.

// object.lua:9 Object:extend(). In C a class is a static Class whose super is set by its own file; what extend's
// metatable chain gives at run time (a method not found in the class is looked up in its super) is done here once:
// every method the class leaves NULL is taken from its super, the super being resolved first.
// note: the "__" keys extend copies (__index, __call) have no C counterpart: NEW() is __call.
void class_init(Class *cls) {
    if (!cls || cls->ready) return;
    cls->ready = true;
    Class *super = cls->super;
    if (!super) return;
    class_init(super);
#define INHERIT(method) if (!cls->method) cls->method = super->method
    INHERIT(update);
    INHERIT(move);
    INHERIT(draw);
    INHERIT(remove);
    INHERIT(hover);
    INHERIT(stop_hover);
    INHERIT(click);
    INHERIT(release);
    INHERIT(drag);
    INHERIT(stop_drag);
    INHERIT(can_drag);
    INHERIT(animate);
    INHERIT(collides_with_point);
    INHERIT(set_container);
    INHERIT(hard_set_T);
    INHERIT(hard_set_VT);
    INHERIT(juice_up);
    INHERIT(put_focused_cursor);
    INHERIT(draw_self);
    INHERIT(set_sprite_pos);
    INHERIT(reset);
    INHERIT(get_pos_pixel);
#undef INHERIT
}

// Every class declared in include/*.h ("extern Class"), in the order of the Lua's load where it matters to nobody:
// class_init resolves the supers itself.
void classes_init(void) {
    static Class *const all[] = {
        // node.h
        &Object_class, &Node_class, &Moveable_class, &Sprite_class, &AnimatedSprite_class, &Particles_class,
        // event.h, controller.h, game.h
        &Event_class, &EventManager_class, &Controller_class, &Game_class,
        // ui.h
        &UIBox_class, &UIElement_class, &DynaText_class,
        // card.h, cardarea.h, blind.h, back.h, tag.h, card_character.h
        &Card_class, &CardArea_class, &Blind_class, &Back_class, &Tag_class, &Tag_sprite_class,
        &Card_Character_class, &Card_Character_card_class,
        // UI_definitions.h
        &UIDEF_blind_sprite_class,
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) class_init(all[i]);
}

// object.lua:22 Object:is(T) is is_a() and object.lua:33 Object:__call(...) is NEW() + the class's init: both are
// declared in core.h and implemented in src/core.c.
