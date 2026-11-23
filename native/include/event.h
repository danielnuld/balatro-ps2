// engine/event.lua: Event (one deferred step: a callback, a delay or a tween) and EventManager (the five queues
// G.E_MANAGER runs once per 1/60 s). Implemented in src/event.c.
//
// How the game's Lua maps (docs/conventions.md, "Functions as values"):
//   G.E_MANAGER:add_event(Event({trigger = 'after', delay = 0.2, func = function() ... return true end}))
//   static bool ev_name(void *ctx) { struct ev_name_ctx *c = ctx; ...; return true; }
//   E_add(G->E_MANAGER, Event_new((EventCfg){.trigger = "after", .delay = 0.2, .func = ev_name, .ctx = c}), NULL, false);
//
// THREE THINGS DIFFER FROM THE LUA'S CONFIG TABLE, because a C struct cannot tell "absent" from false / 0:
//   1. blocking and blockable default to TRUE in the Lua, so the struct has the INVERTED members not_blocking and
//      not_blockable (zero = the Lua's default). `blocking = false` is `.not_blocking = true`; `blocking = true` is
//      nothing; `blockable = (x == false)` is `.not_blockable = !(x == false)`. There is no member called
//      blocking or blockable in EventCfg: writing one does not compile, on purpose.
//   2. The func of an 'ease' event takes and returns a number (function(t) return math.floor(t) end): it is the
//      separate member ease_func. `.func` is only for the other triggers.
//   3. ref_table + ref_value name a number somewhere. In C that is ONE of: a Table and a string key
//      (.ref_table, .ref_value), a Table and a 1-based index (.ref_table, .ref_index), the address of a float
//      (.ref_float: a struct member such as &card->moveable.node.T->scale or &particles->fade_alpha, or a colour
//      channel: colour[4] of the Lua is &colour[3]), or the address of a num (.ref_num). A field that lives in an
//      object's dyn table is (.ref_table = obj->dyn, .ref_value = "name").
#ifndef EVENT_H
#define EVENT_H
#include "types.h"
#include "node.h"

// config.func of the triggers 'immediate', 'after', 'before', 'condition': returns the Lua's truthiness of its
// result (true: the event is complete; false or no return: it is called again on the next pass).
typedef bool (*EventFunc)(void *ctx);
// config.func of the trigger 'ease': gets the interpolated value, returns the value to store.
typedef num (*EaseFunc)(num t, void *ctx);

// ---------------------------------------------------------------- EventCfg: the table passed to Event({...})
// Zero / NULL in every member is the Lua's "key absent". Build it with designated initializers.
typedef struct EventCfg {
    const char *trigger;   // 'immediate', 'after', 'before', 'ease', 'condition'; NULL: 'immediate'
    num delay;             // seconds of `timer`; 0 as nil (the default is 0)
    bool not_blocking;     // INVERTED config.blocking: false (default) = blocking = true. See 1. above
    bool not_blockable;    // INVERTED config.blockable: false (default) = blockable = true. See 1. above
    EventFunc func;        // config.func for every trigger but 'ease'; NULL: returns true ('condition': the comparison)
    EaseFunc ease_func;    // config.func of an 'ease'; NULL: function(t) return t end
    void *ctx;             // passed to func / ease_func: what the closure captured (NEWCTX), or NULL
    // the number an 'ease' animates or a 'condition' watches: exactly one of the four forms (see 3. above);
    // all NULL with trigger 'ease' is the Lua's crash on a nil ref_table: ERROR()
    Table *ref_table;      // config.ref_table when it is a table (G->GAME, card->ability, obj->dyn...); NULL
    const char *ref_value; // config.ref_value when it is a string key of ref_table; NULL
    int ref_index;         // config.ref_value when it is a number: the 1-based index in ref_table; 0: not used
    float *ref_float;      // the address of the float, when ref_table is a struct or a Colour; NULL
    num *ref_num;          // the address of the num, when ref_table is a struct with a num member; NULL
    const char *ease;      // config.ease: 'lerp', 'elastic', 'quad'; NULL: 'lerp'
    num ease_to;           // config.ease_to: the value the ease ends at
    Val stop_val;          // config.stop_val ('condition' only, unused by the game); zero is NIL
    const char *timer;     // config.timer: a key of G.TIMERS ('REAL', 'TOTAL', 'UPTIME'); NULL: 'REAL' if the
                           // event is created on pause (or pause_force), else 'TOTAL'
    bool no_delete;        // config.no_delete: EventManager_clear_queue keeps it
    bool start_timer;      // config.start_timer (no caller passes it): the delay counts from creation
    bool pause_force;      // config.pause_force: runs while the game is paused
} EventCfg;
// Keys the Lua's Event does NOT read, although callers pass them: `type` (ease_value, common_events.lua:271, passes
// the ease type as `type`: it is ignored, so those eases are always 'lerp': do not "fix" it by setting .ease),
// `created_on_pause` (computed, never a config key).

// What Event:handle writes in _results (the Lua's scratch table G.ARGS.event_manager_update: it does not exist in C)
typedef struct EventResults {
    bool blocking;   // the event's blocking
    bool completed;  // func returned truthy now or before
    bool time_done;  // its delay has passed
    bool pause_skip; // not handled: created unpaused and the game is paused
} EventResults;

// ---------------------------------------------------------------- Event (engine/event.lua:2)
struct Event {
    Object object;
    const char *trigger;     // 'immediate', 'after', 'before', 'ease', 'condition' (interned: compare with streq)
    bool blocking;           // while it is in the queue, the blockable events behind it are not handled; default true
    bool blockable;          // an earlier blocking event holds it back; default true
    bool complete;           // func has returned truthy, or the ease has ended
    bool start_timer;        // false: `time` is stamped at the first handle (and this becomes true)
    EventFunc func;          // never NULL after init for the triggers that call it (the defaults are in event.c)
    EaseFunc ease_func;      // self.func of an 'ease'; never NULL after init for an 'ease'
    void *ctx;               // the argument of func / ease_func; NULL
    num delay;               // seconds of `timer`
    bool no_delete;          // survives clear_queue (nil and false are the same here)
    int created_on_pause;    // config.pause_force or G.SETTINGS.paused: 1 true, 0 false, -1 nil. Only 0 is skipped
                             // on pause (the Lua tests `== false`; G.SETTINGS.paused is nil at boot, game.lua:40)
    const char *timer;       // the key of G.TIMERS the delay is measured on
    num time;                // G.TIMERS[timer] at creation, stamped again at the first handle unless start_timer
    // self.ease: only for trigger 'ease' (the ref members are also self.condition's, for trigger 'condition')
    struct {
        const char *type;      // 'lerp', 'elastic', 'quad'
        Table *ref_table;      // the four forms of EventCfg: one is set
        const char *ref_value;
        int ref_index;
        float *ref_float;
        num *ref_num;
        num start_val;         // read at init and again at the first handle
        num end_val;           // config.ease_to
        bool started;          // start_time ~= nil
        num start_time, end_time; // of `timer`; valid when started
    } ease;
    Val stop_val;            // self.condition.stop_val
};
// in dyn: nothing (every field of the Lua's Event is a member; self.condition is ease's ref members + stop_val).
// Lifetime: the pointer given to ref_float / ref_num must stay valid while the event is queued (the Lua keeps
// ref_table alive; in C an object freed by its remove while one of its eases is still running is a dangling
// pointer: an ease on a struct member of an object that can be removed earlier needs a `// note:` at the call).
// Events are not freed by hand, like tables (docs/conventions.md, Memory): clear_queue may run inside the func of the
// very event it removes. Do not keep a pointer to an event after E_add.
// ponytail: events and their ctx leak like tables do; a pool with free-after-the-pass when the 32 MB matter.

// event.lua:5 Event:init(config). Reads G.SETTINGS.paused and G.TIMERS. config is never NULL.
void Event_init(Event *self, const EventCfg *config);
// Event({...}): NEW(Event) + Event_init. Never returns NULL.
// (through a pointer: the PS2's compiler dies passing this struct by value. Written as before:
// Event_new((EventCfg){...}).)
Event *Event_new_p(const EventCfg *config);
#define Event_new(...) Event_new_p(&(__VA_ARGS__))
// event.lua:48 Event:handle(_results). _results is never NULL; the caller clears it first (EventManager_update).
void Event_handle(Event *self, EventResults *_results);

// ---------------------------------------------------------------- EventManager (engine/event.lua:106)
struct EventManager {
    Object object;
    // self.queues: name -> T(array of O(Event *)), never NULL. The five names, inserted in this order, which is
    // the order `pairs(self.queues)` walks them in C: 'unlock', 'base', 'tutorial', 'achievement', 'other'.
    // #G.E_MANAGER.queues.unlock is tlen(ttab(G->E_MANAGER->queues, "unlock")).
    Table *queues;
    num queue_timer;          // seconds: G.TIMERS.REAL at init, += dt every update
    num queue_dt;             // 1/60: the least time between two passes over the queues
    num queue_last_processed; // seconds: advances by queue_dt on each pass that is not forced
};
// in dyn: nothing.

// event.lua:109 EventManager:init(). G.E_MANAGER = EventManager() is NEW(EventManager) + this.
void EventManager_init(EventManager *self);
// event.lua:122 EventManager:add_event(event, queue, front). queue NULL: 'base'. front false as nil: appended;
// true: inserted first. event NULL or not an Event: ignored, as the Lua's event:is(Event).
void EventManager_add_event(EventManager *self, Event *event, const char *queue, bool front);
// The name docs/conventions.md uses for add_event: E_add(G->E_MANAGER, Event_new((EventCfg){...}), NULL, false).
static inline void E_add(EventManager *self, Event *event, const char *queue, bool front) {
    EventManager_add_event(self, event, queue, front);
}
// event.lua:133 EventManager:clear_queue(queue, exception). Both NULL as nil. queue NULL: every queue is cleared
// (exception is then ignored, as in the Lua: button_callbacks.lua:3006 passes one and it does nothing); queue and
// exception given: every queue but `exception`; only queue: that queue. Events with no_delete stay.
void EventManager_clear_queue(EventManager *self, const char *queue, const char *exception);
// event.lua:171 EventManager:update(dt, forced). dt: real seconds (G.real_dt). forced false as nil; true runs the
// pass now without advancing queue_last_processed (button_callbacks.lua:1377 calls update(0, true)).
void EventManager_update(EventManager *self, num dt, bool forced);

extern Class Event_class, EventManager_class;
#endif
