// engine/event.lua:1-195: Event and EventManager.
#include "all.h"

// event.lua:2, :106
Class Event_class = {.name = "Event", .super = &Object_class};
Class EventManager_class = {.name = "EventManager", .super = &Object_class};

// G.TIMERS[self.timer] read as a number.
// note: the Lua only crashes on an unknown timer name when it does arithmetic with the nil; here any read of one
// is the error (the game uses 'REAL', 'TOTAL' and 'UPTIME' only).
static num timer_now(const char *timer) {
    num *t = TIMERS_get(timer);
    if (!t) ERROR("event.lua: G.TIMERS['%s'] is nil", timer ? timer : "nil");
    return *t;
}

// ref_table[ref_value] as a Lua value, through whichever of the four forms of event.h is set.
static Val ref_get(Event *self) {
    if (self->ease.ref_float) return N((num)*self->ease.ref_float);
    if (self->ease.ref_num) return N(*self->ease.ref_num);
    if (self->ease.ref_table && self->ease.ref_value) return tget(self->ease.ref_table, self->ease.ref_value);
    if (self->ease.ref_table && self->ease.ref_index) return tgeti(self->ease.ref_table, self->ease.ref_index);
    ERROR("event.lua: attempt to index a nil ref_table (trigger '%s')", self->trigger); // the Lua's crash
}

// ref_table[ref_value] = v
static void ref_set(Event *self, num v) {
    if (self->ease.ref_float) *self->ease.ref_float = (float)v;
    else if (self->ease.ref_num) *self->ease.ref_num = v;
    else if (self->ease.ref_table && self->ease.ref_value) tset(self->ease.ref_table, self->ease.ref_value, N(v));
    else if (self->ease.ref_table && self->ease.ref_index) tseti(self->ease.ref_table, self->ease.ref_index, N(v));
    else ERROR("event.lua: attempt to index a nil ref_table (trigger '%s')", self->trigger);
}

// event.lua:19 function() return true end
static bool ev_default_true(void *ctx) { return true; }
// event.lua:35 function(t) return t end
static num ev_default_ease(num t, void *ctx) { return t; }
// event.lua:43 function() return self.condition.ref_table[self.condition.ref_value] == self.condition.stop_val end
static bool ev_default_condition(void *ctx) {
    Event *self = ctx;
    return veq(ref_get(self), self->stop_val);
}

// event.lua:5 Event:init(config)
void Event_init(Event *self, const EventCfg *config) {
    self->trigger = config->trigger ? str(config->trigger) : "immediate";
    // :7-16 blocking / blockable default to true (EventCfg holds them inverted)
    self->blocking = !config->not_blocking;
    self->blockable = !config->not_blockable;
    self->complete = false;
    self->start_timer = config->start_timer;
    self->func = config->func ? config->func : ev_default_true;
    self->ctx = config->ctx;
    self->delay = config->delay;
    self->no_delete = config->no_delete;
    // :22 config.pause_force or G.SETTINGS.paused (1 true, 0 false, -1 nil)
    if (config->pause_force) self->created_on_pause = 1;
    else {
        Val paused = tget(G->SETTINGS, "paused");
        self->created_on_pause = paused.t == V_NIL ? -1 : (truthy(paused) ? 1 : 0);
    }
    self->timer = config->timer ? str(config->timer) : (self->created_on_pause == 1 ? "REAL" : "TOTAL");

    if (streq(self->trigger, "ease")) {
        self->ease.type = config->ease ? str(config->ease) : "lerp";
        self->ease.ref_table = config->ref_table;
        self->ease.ref_value = config->ref_value;
        self->ease.ref_index = config->ref_index;
        self->ease.ref_float = config->ref_float;
        self->ease.ref_num = config->ref_num;
        // note: a nil ref_table[ref_value] is start_val = nil in the Lua (it crashes later, in the arithmetic); here 0
        self->ease.start_val = vnum(ref_get(self));
        self->ease.end_val = config->ease_to;
        self->ease.started = false; // start_time = nil, end_time = nil
        self->ease_func = config->ease_func ? config->ease_func : ev_default_ease;
    }
    if (streq(self->trigger, "condition")) {
        // self.condition: the ref members of ease + stop_val (event.h)
        self->ease.ref_table = config->ref_table;
        self->ease.ref_value = config->ref_value;
        self->ease.ref_index = config->ref_index;
        self->ease.ref_float = config->ref_float;
        self->ease.ref_num = config->ref_num;
        self->stop_val = config->stop_val;
        if (config->func) self->func = config->func;
        else {
            // note: the default closure captures self: it replaces config.ctx, which has no func to go to
            self->func = ev_default_condition;
            self->ctx = self;
        }
    }
    self->time = timer_now(self->timer);
}

// Event({...})
Event *Event_new_p(const EventCfg *config) {
    Event *self = NEW(Event);
    Event_init(self, config);
    return self;
}

// event.lua:48 Event:handle(_results)
void Event_handle(Event *self, EventResults *_results) {
    _results->blocking = self->blocking, _results->completed = self->complete;
    if (self->created_on_pause == 0 && SETTINGS_snap()->paused) { _results->pause_skip = true; return; }
    if (!self->start_timer) { self->time = timer_now(self->timer); self->start_timer = true; }
    if (streq(self->trigger, "after")) {
        if (self->time + self->delay <= timer_now(self->timer)) {
            _results->time_done = true;
            _results->completed = self->func(self->ctx);
        }
    }
    if (streq(self->trigger, "ease")) {
        if (!self->ease.started) {
            self->ease.started = true;
            self->ease.start_time = timer_now(self->timer);
            self->ease.end_time = timer_now(self->timer) + self->delay;
            self->ease.start_val = vnum(ref_get(self));
        }
        if (!self->complete) {
            if (self->ease.end_time >= timer_now(self->timer)) {
                num percent_done = ((self->ease.end_time - timer_now(self->timer)) / (self->ease.end_time - self->ease.start_time));

                if (streq(self->ease.type, "lerp")) {
                    ref_set(self, self->ease_func(percent_done * self->ease.start_val + (1 - percent_done) * self->ease.end_val, self->ctx));
                }
                if (streq(self->ease.type, "elastic")) {
                    percent_done = -pow(2, 10 * percent_done - 10) * sin((percent_done * 10 - 10.75) * 2 * M_PI / 3);
                    ref_set(self, self->ease_func(percent_done * self->ease.start_val + (1 - percent_done) * self->ease.end_val, self->ctx));
                }
                if (streq(self->ease.type, "quad")) {
                    percent_done = percent_done * percent_done;
                    ref_set(self, self->ease_func(percent_done * self->ease.start_val + (1 - percent_done) * self->ease.end_val, self->ctx));
                }
            } else {
                ref_set(self, self->ease_func(self->ease.end_val, self->ctx));
                self->complete = true;
                _results->completed = true;
                _results->time_done = true;
            }
        }
    }
    if (streq(self->trigger, "condition")) {
        if (!self->complete) _results->completed = self->func(self->ctx);
        _results->time_done = true;
    }
    if (streq(self->trigger, "before")) {
        if (!self->complete) _results->completed = self->func(self->ctx);
        if (self->time + self->delay <= timer_now(self->timer)) {
            _results->time_done = true;
        }
    }
    if (streq(self->trigger, "immediate")) {
        _results->completed = self->func(self->ctx);
        _results->time_done = true;
    }
    if (_results->completed) self->complete = true;
}

// event.lua:109 EventManager:init()
void EventManager_init(EventManager *self) {
    self->queues = TAB(
        "unlock", T(tnew()),
        "base", T(tnew()),
        "tutorial", T(tnew()),
        "achievement", T(tnew()),
        "other", T(tnew()));
    self->queue_timer = G->TIMERS.REAL;
    self->queue_dt = 1.0 / 60;
    self->queue_last_processed = G->TIMERS.REAL;
}

// self.queues[queue]; an unknown name is the Lua's crash on a nil queue
static Table *queue_of(EventManager *self, const char *queue) {
    Table *q = ttab(self->queues, queue);
    if (!q) ERROR("event.lua: self.queues['%s'] is nil", queue ? queue : "nil");
    return q;
}

// event.lua:122 EventManager:add_event(event, queue, front)
void EventManager_add_event(EventManager *self, Event *event, const char *queue, bool front) {
    if (!queue) queue = "base";
    if (is_a(event, &Event_class)) {
        if (front) {
            tinsert_at(queue_of(self, queue), 1, O(event));
        } else {
            Table *q = queue_of(self, queue);
            tseti(q, tlen(q) + 1, O(event));
        }
    }
}

// event.lua:133 EventManager:clear_queue(queue, exception)
void EventManager_clear_queue(EventManager *self, const char *queue, const char *exception) {
    if (!queue) {
        // clear all queues
        for (TIter it = titer(self->queues); tnext(&it);) {
            Table *v = it.val.tab;
            int i = 1;
            while (i <= tlen(v)) {
                if (!((Event *)tobji(v, i))->no_delete) {
                    tremove(v, i);
                } else {
                    i = i + 1;
                }
            }
        }
    } else if (exception) { // clear all but exception
        for (TIter it = titer(self->queues); tnext(&it);) {
            Table *v = it.val.tab;
            if (!streq(vstr(it.key), exception)) {
                int i = 1;
                while (i <= tlen(v)) {
                    if (!((Event *)tobji(v, i))->no_delete) {
                        tremove(v, i);
                    } else {
                        i = i + 1;
                    }
                }
            }
        }
    } else {
        int i = 1;
        while (i <= tlen(queue_of(self, queue))) {
            if (!((Event *)tobji(queue_of(self, queue), i))->no_delete) {
                tremove(queue_of(self, queue), i);
            } else {
                i = i + 1;
            }
        }
    }
}

// event.lua:171 EventManager:update(dt, forced)
void EventManager_update(EventManager *self, num dt, bool forced) {
    self->queue_timer = self->queue_timer + dt;
    if (self->queue_timer >= self->queue_last_processed + self->queue_dt || forced) {
        self->queue_last_processed = self->queue_last_processed + (forced ? 0 : self->queue_dt);
        // note: pairs(self.queues) is hash order in the Lua; here unlock, base, tutorial, achievement, other (event.h)
        for (TIter it = titer(self->queues); tnext(&it);) {
            Table *v = it.val.tab;
            bool blocked = false;
            int i = 1;
            while (i <= tlen(v)) {
                // G.ARGS.event_manager_update is a local here (event.h)
                EventResults results;
                results.blocking = false, results.completed = false, results.time_done = false, results.pause_skip = false;
                // v[i] is read by index again for the removal: the func may have changed the queue
                if (!blocked || !((Event *)tobji(v, i))->blockable) Event_handle((Event *)tobji(v, i), &results);
                if (results.pause_skip) {
                    i = i + 1;
                } else {
                    if (!blocked && results.blocking) blocked = true;
                    if (results.completed && results.time_done) {
                        tremove(v, i);
                    } else {
                        i = i + 1;
                    }
                }
            }
        }
    }
}
