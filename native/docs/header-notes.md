# Header notes

Notes on the headers in include/: what each one models from the game's Lua and the decisions behind its types.
They are the decisions that are not obvious from the header itself.

Each section ends with the points that were open when that header was written, before the headers and sources
that settle them existed; `docs/source-notes.md` says how each file dealt with them, and `docs/conventions.md`
has the rules all of them follow.

## controller.h

**Counts**
- 4 structs: `Controller` (base `Object` first), plus `ControllerTarget`, `ControllerCursor` and `AxisButton`.
- 36 prototypes, one for every method in `engine/controller.lua`, in file order, each with its Lua line and nil conventions.
- `extern Class Controller_class`. The file has no other file-level state.

**Design decisions**
- **Names stay strings.** Buttons, keys, HID types, directions and `snap_cursor_to.type` are the Lua's strings, compared with `streq`. Where the Lua uses `''` the C uses `""`, never NULL (`axis_buttons.*.current/previous`, `HID.last_type`, `GAMEPAD_CONSOLE`). They are not enums: this follows `docs/conventions.md` ("same keys as the Lua").
- **Run-time-keyed maps are `Table *`, never NULL.** This covers the pressed/held/released sets, `held_key_times`, `held_button_times`, `locks`, `button_registry`, `cardarea_context`, `cursor_context.stack`, `collision_list` and `nodes_at_cursor`.
  - `held_button_times`: absent is not 0; test with `tget`.
  - `locks` also takes a Tag's ID as a number key (tag.lua:132): use `tsetv(locks, N(id), ...)`.
  - `collision_list` and `nodes_at_cursor` are arrays of `O(Node)`. `nodes_at_cursor` must be created in `Controller_init` (the Lua creates it lazily with `EMPTY(nil)`).
  - `EMPTY(t)` is `tclear`.
- **Fixed-shape records are structs.**
  - The five targets are `ControllerTarget`.
  - `cursor_down`, `cursor_up` and `cursor_hover` are `ControllerCursor`. `T` is a `Vec2` by value, and `time` is `num` because misc_functions.lua:238 seeds from `cursor_hover.time`.
  - `cursor_hover.prev_target` and `interrupt.stack` are members.
- **nil collapsed to false or 0 where the Lua only tests truthiness.**
  - Bools: `interrupt.focus` (button_callbacks.lua:2301 sets nil, so write `false`), `interrupt.stack`, `locked`, `frame_buttonpress`, `COYOTE_FOCUS`, `no_holdcap`, `capslock`.
  - Floats: `overlay_timer` 0 as nil; `repress_timer` 0 as nil (it is only ever 0.3 or 0.1).
- **`snap_cursor_to` and `L_cursor_queue` are structs with a `set` flag.** A context node that was removed (`false` in the Lua) is `node = NULL`, giving type `"transform"`.
- **`Controller_snap_to(self, Table *args)`** keeps the args table per `docs/conventions.md`: `TAB("node", O(x))`, with `T` as `{x=, y=}`.
- **`Controller_recall_cardarea_focus(self, Val)`** takes `S(name)` or `O(CardArea *)`; the type really varies (button_callbacks.lua:2305).
- **Cursor press functions take `Val x, Val y`**, with `NIL` meaning default to `cursor_position`. This applies to `L_cursor_press`, `L_cursor_release` and both `queue_*_cursor_press`.
- **`capture_focused_input` returns `bool`.** The Lua's nil return (:1327, :1336) is false; the only caller that reads it tests `not`.
- **`update_axis` returns `const char *`**, NULL as nil, passed straight to `set_HID_flags(type, NULL)`.
- **`get_cursor_collision` takes `Vec2` by value** (the x and y of `G.CURSOR.T`).
- **Scratch is local.** The `G.ARGS.focus_*` tables, `G.ARGS.gamepad_patterns` and the accidental global `_ft` are locals of `controller.c`. The three event closures (:199, :400, :812) are statics there; :812 is debug-only.
- **`pairs` order.** `handle_axis_buttons` walks `l_stick, r_stick, l_trig, r_trig` (struct order). The loops over the pressed/held/released maps run in `titer` insertion order where the Lua's is hash order; that needs a `// note:`.
- **In dyn, not members:** `last_touch_time`, `using_touch`, `lock_input`, `focus_cursor_stack`, `focus_cursor_stack_level`.
- **Joystick.** `GAMEPAD.object` is `Joystick *`. The header repeats `typedef struct Joystick Joystick;`, which is a legal duplicate of the one in `love.h`. `keyboard_controller` is a `Joystick *` left NULL, since it only serves the debug keybinds.

**Open points when the header was written**
- **Fields on other classes.** The controller reads `click_timeout`, `area`, `rank`, `facing`, `jimbo` (Card), `UIBox` (UIElement), `children[i]`, `config.focus_args` and `G.CURSOR`, `G.ROOM`, `G.DRAW_HASH`, `G.MOVEABLES`, `G.VIBRATION`, `G.DEADZONE`. Whether these are members or dyn depends on `card.h`, `ui.h` and `game.h`, which were written later.
- **`node:click()` at :456.** The Lua tests `registry[i].node.click` for existence; with `Class`, `click` is always inherited, so the test is always true in C.
- **`put_focused_cursor` returning nothing** (the UIElement tab case with no chosen tab) is an arithmetic error in the Lua. The C `Class.put_focused_cursor(self, float *x, float *y)` cannot signal "no values".
- **Console matching.** `get_console_from_gamepad` uses Lua patterns with `%f` frontiers, and `core.h` has no pattern matcher. With `love.h`'s fixed mapping string `"0,PS2 Controller,"` the name is "PS2 Controller", so only the `PS%d` pattern matters; the source hand-codes the checks or uses `TODO_PORT`.

## io.h

**Counts:** 0 structs (none of the three Lua files defines a class), 22 prototypes, 2 externs (`SOURCES`, `DISABLE_SFX`).
- **Sound (9):** `sound_manager_init`, `PLAY_SOUND`, `STOP_AUDIO`, `SET_SFX`, `MODULATE`, `RESTART_MUSIC`, `AMBIENT`, `RESET_STATES`, `sound_manager_request`.
- **Save (6):** `save_manager_request`, `save_manager_save_progress`, `save_manager_save_settings`, `save_manager_save_metrics`, `save_manager_save_notify`, `save_manager_save_run`.
- **Packer (7):** `STR_PACK`, `STR_UNPACK`, `get_compressed`, `compress_and_save`, `compress_and_save_table`, `table_pack`, `table_unpack`.

**Design decisions**

1. **Name collision with `misc_functions.lua` (link risk).** Lines 1892-2022 of that file define the same seven globals (`PLAY_SOUND`, `STOP_AUDIO`, `SET_SFX`, `MODULATE`, `RESTART_MUSIC`, `AMBIENT`, `RESET_STATES`) as the never-taken `F_SOUND_THREAD = false` fallback. `io.h` declares them as the port's only definitions, in `src/sound_manager.c`. If `misc_functions.c` defines them too, the link fails with duplicate symbols. If `misc_functions.h` declares them with different signatures, the compile fails. The signatures here are:
   - `Table *PLAY_SOUND(Table *args)`
   - `void STOP_AUDIO(void)`
   - `void SET_SFX(Table *s, Table *args)`
   - `void MODULATE(Table *args)`, `void RESTART_MUSIC(Table *args)`, `void AMBIENT(Table *args)`
   - `void RESET_STATES(int state)`

2. **`RESET_STATES(G.STATES.MENU)` at `game.lua:1524` must stay a no-op.** On PC it touches the main thread's own `SOURCES` (`game.lua:64-73`, lists that stay empty). With a single `SOURCES` in the port, calling this one would change live voices, so that call site should be a comment.

3. **Voice records and `SOURCES` are plain Tables with the Lua's keys.** `SOURCES` is `extern Table *SOURCES`; the audio handle is stored as `P(Source *)` under `"sound"`. If `game.h` also declares `SOURCES` for `game.lua:64`, it must be `Table *`.

4. **Requests are executed inline.** `G.SOUND_MANAGER.channel:push(t)` becomes `sound_manager_request(t)`, and `G.SAVE_MANAGER.channel:push(t)` becomes `save_manager_request(t)` (or the typed `save_manager_save_*` functions).
   - Sound sites: `misc_functions.lua:712`, `:776`, `main.lua:122`. Save sites: `game.lua:1218`, `:2693-2717`.
   - The "modulate" dispatch runs `MODULATE` and then `AMBIENT`, so `modulate_sound` must go through `sound_manager_request`, not call `MODULATE` directly.
   - The request table is used in place, not copied. `PLAY_SOUND` and `RESTART_MUSIC` write `per`, `vol` and `sound_code` into it; this is harmless.

5. **`sound_manager_init()` goes where `game.lua:87` starts the thread.** It is the top-level preload of `sound_manager.lua:1-35`.

6. **Save functions are prefixed `save_manager_`.** `save_run()` already exists as a global in `misc_functions.lua:1452`. `profile_num` is a `Val` (NIL defaults to 1) for notify and run, and a `num` for settings, where the Lua has no default.

7. **The packed format stays the Lua source text (`return {[k]=v,}`), not a binary format.** Callers pass the literal `'return {}'` to `STR_UNPACK` (`game.lua:740`, `save_manager.lua:28`), and `get_compressed` results are also read as plain text (`unlock_notify`). This lets every string be a `const char *` and lets PC saves load.

8. **`compress_and_save` is split in two.** `compress_and_save(file, const char *)` takes text and `compress_and_save_table(file, Table *)` takes a table. `table_pack` / `table_unpack` are the buffer-owning variants (malloc and free, no forever-living string), meant for the periodic saves.

9. **`STR_PACK` writes any object value (`V_OBJ`) as `"MANUAL_REPLACE"`.** This is the Lua's `v:is(Object)` test.

**Open points when the header was written**
- What `misc_functions.h` and `game.h` declare for the duplicated sound globals and `SOURCES`; neither header existed yet.
- The argument type others will use for `RESET_STATES`; it is `int` here, and a `num` or a states enum elsewhere would conflict.
- Whether `G.SETTINGS` and `G.PROFILES[n]` are Tables in `game.h`; the save functions assume they are.
- The `love.h` names for file and deflate calls (`love_filesystem_*`, `love_data_compress`); they are not referenced in the header, only needed by the `.c` files.

## event.h

**Contents**
- 4 structs: `EventCfg`, `EventResults`, `Event`, `EventManager`. The last two have `Object` as first member; nothing is left in dyn for either class.
- 2 function typedefs: `EventFunc` (`bool (*)(void *ctx)`) and `EaseFunc` (`num (*)(num t, void *ctx)`).
- 8 prototypes in file order: `Event_init`, `Event_new`, `Event_handle`, `EventManager_init`, `EventManager_add_event`, `E_add` (static inline wrapper of `add_event`), `EventManager_clear_queue`, `EventManager_update`.
- `extern Class Event_class, EventManager_class;`. The file has no other file-level state; `G.E_MANAGER` belongs to `game.h`.

**Design decisions**
1. **`blocking` / `blockable` are inverted.** They default to true in the Lua and a zeroed struct cannot tell absent from false, so `EventCfg` has `.not_blocking` and `.not_blockable`. `blocking = false` becomes `.not_blocking = true`. Writing `.blocking` does not compile, on purpose. This departs from the "same fields as the Lua" rule in `docs/conventions.md`. The `Event` struct itself keeps the real `blocking` / `blockable`.
2. **Ease callback is a separate member.** The func of an `'ease'` takes and returns a number, so it goes in `.ease_func` (NULL is identity); `.func` is for the other triggers.
3. **`ref_table` + `ref_value` has four forms; set exactly one.**
   - `.ref_table` + `.ref_value`: a Table and a string key. A field kept in an object's dyn is `.ref_table = obj->dyn`.
   - `.ref_table` + `.ref_index`: a Table and a 1-based index.
   - `.ref_float`: the address of a float (a struct member, or a colour channel: Lua `colour[4]` is `&colour[3]`).
   - `.ref_num`: the address of a num.
4. **Strings, not enums.** `trigger`, `timer`, `ease` and queue names are `const char *` as in the `docs/conventions.md` example (NULL is the Lua default), not enums. `delay`, `ease_to`, `time` and the `queue_*` fields are `num`; `EventManager_update` takes `float dt`.
5. **`created_on_pause` is an `int`: 1, 0, or -1 for nil.** The Lua tests `== false`, and `G.SETTINGS.paused` is nil at boot (`game.lua:40`), so events created then are never skipped on pause. It is not an `EventCfg` key; the key is `pause_force`.
6. **`ease_value` always lerps.** It passes the ease type under the key `type` (`common_events.lua:271`), which `Event` never reads. The header says not to "fix" this by setting `.ease`.
7. **`queues` is a `Table *`.** It maps name to an array of `O(Event *)`, inserted in the order unlock, base, tutorial, achievement, other; that is the order `pairs` walks in C. `#G.E_MANAGER.queues.unlock` is `tlen(ttab(G->E_MANAGER->queues, "unlock"))`.
8. **`Event_handle` takes an `EventResults *`.** The scratch table `G.ARGS.event_manager_update` does not exist.
9. **Events are never freed** (marked with a `ponytail:` comment). `clear_queue` can run inside the func of the event it removes, so freeing there would be a use-after-free. Nobody in the game keeps an event pointer (no `= Event(` anywhere).
10. **`clear_queue(NULL, exception)` clears everything**, as in the Lua; the single caller that passes an exception (`button_callbacks.lua:3006`) gets no effect from it.

**Open points when the header was written**
- **`G.TIMERS` and `G.SETTINGS.paused` types.** `game.h` did not exist yet. `event.c` needs `G.TIMERS[name]` by string key and a nil-distinguishable `paused`.
- **Real `pairs` order of the five queues** in LuaJIT. The header uses constructor order. Queues do not block each other, so it only changes the order of side effects between queues within one pass.
- **Dangling `ref_float` / `ref_num`.** If an object is freed by its `remove` while one of its eases is still queued, the pointer dangles. Candidates are the dissolve eases in `card.lua` and `blind.lua`, `Particles_fade`, and the `card.T` eases. The header asks for a `// note:` at such call sites; a real fix depends on how `card.h` and `blind.h` free objects.

## card.h

**Counts**
- 1 struct (`Card`, with `Moveable` as first member) and `extern Class Card_class`.
- 70 prototypes: the 67 methods of `card.lua` in file order, plus `Card_calculate_joker_a`, `_b` and `_c`.
- 2 static inline accessors: `Card_child` and `Card_field`.
- No file-level state: `card.lua` has none, and `G.sort_id` belongs to `Game`.

**Design decisions**
- **Tables, not enums.** `ability`, `base`, `edition` and `params` are `Table *` members with the Lua's keys; `config` is the Node's `moveable.node.config`. An enum/struct mapping was considered and dropped: `docs/conventions.md` asks for tables.
- **Children** stay by name in `node.children`; `Card_child(self, "center")` fetches one. The names and their types are listed in the header comment.
- **Strings as the Lua has them.** `facing`, `sprite_facing`, `flipping`, `seal`, `sticker`, `sticker_run`, `back` and `label` are `const char *`, NULL as nil; compare with `streq`.
- **`sell_cost_label` is a `Val`** (`N(sell_cost)` or `S("?")`). The UI binds `ref_table = card` with `ref_value = 'sell_cost_label'` and `'cost'`, so `Card_field(card, key)` reads a card by name (those members, else `dyn`). The UI code calls it when a `ref_table` is a Card.
- **Nil collapsed to a zero value:**
  - `rank` and `playing_card` are `int`, 0 as nil.
  - `added_to_deck` and `debuff` are `bool`.
  - `dissolve` is `float`, 0 as nil.
  - `Card_get_id` returns `num`, 0 when the card has no rank.
- **`Card_set_debuff` takes `bool`.** The one call without an argument (card.lua:2283) passes false; it always takes the perishable early return, which never reads the argument.
- **Sentinels for optional numbers:**
  - `Card_hard_set_T`: NAN for each nil of X, Y, W, H; `card:hard_set_T()` is four NANs.
  - `explode_time_fac`, `dissolve_time_fac`, `timefac`: `< 0` is nil (default 1).
  - `Card_juice_up(self, float scale, Val rot_amount)`: `scale < 0` is nil, `rot_amount` NIL is nil. This matches the `Class.juice_up` slot.
- **`Val` returns.** `Card_calculate_dollar_bonus` returns `Val` because the caller tests truthiness and 0 is true. `Card_calculate_joker` returns `Val`: a table, `B(true)` (Sixth Sense in `destroying_card`), or NIL.
- **The three joker parts.** `_a` and `_b` return true when a branch's condition held, including its `and not self.getting_sliced` / `and not context.blueprint` part. `*ret` is then the Lua's return, NIL if the branch falls out; false leaves `*ret` untouched. All three run only when `ability.set == 'Joker'`; otherwise `Card_calculate_joker` returns NIL after the Planet check.
- **`Card_apply_to_run` can receive `self == NULL`** (`Card.apply_to_run(nil, center)` in back.lua:179, 236 and game.lua:2095).
- **Colours.** `dissolve_colours` is an array of `P(float *)`, the same shape as `Particles.colours`. The `{{1,1,1,0.8}}` literal in `shatter` needs static storage. `back_overlay` is a `Colour` member that `Card_draw` fills.
- **In `dyn`, not members:** `getting_sliced`, `shattered`, `destroyed`, `removed` (lower case; `REMOVED` is the Node's), `opening`, `shop_voucher`, `vampired`, `lucky_trigger`, `ability_UIBox_table`.
- **Inherited fields are not redeclared:** `shadow_height`, `zoom`, `mouse_damping`, `juice`, `pinch`, `layered_parallax`, `parent`, `CT`, `REMOVED`.

**Open points when the header was written**
- **Ease events.** `event.h` did not exist yet, so how `ref_table = self, ref_value = 'dissolve'` is expressed was open. `dissolve` is a `float` member so a `float *` works; `Particles.scale` is the same case.
- **Shared `states`.** `set_sprites` makes the front, center and back sprites share the card's `states.hover/click/drag` tables. `States` in `node.h` is by value, so the source must mirror those flags or read the parent's.
- **`layered_parallax`.** `set_card_area` shares the area's table by reference and init sets the children's to nil; `Moveable.layered_parallax` is a by-value `Vec2`. A copy is equivalent only if nothing mutates an area's parallax in place; only `engine/ui.lua` was seen doing that, on UI elements.
- **Sprite reads of the draw major.** `Sprite:draw_shader` reads `tilt_var`, `hover_tilt`, `dissolve` and `dissolve_colours` from its `draw_major`, which may be a Card or a Blind. `sprite.c` must test `is_a(..., &Card_class)` and read these members, since they are not in `dyn`.
- **NAN for `hard_set_T`.** `node.h` does not say how `Moveable_hard_set_T` receives nil; `G.deck:hard_set_T()` has the same need. The NAN convention starts here; `node.h` and `cardarea.h` have to follow it.
- **`calculate_joker`.** The header was written from the branch structure and return shapes of card.lua 2337–4040 (the `calculate_joker` body), not from a full reading. The `ability` key list in the header comment may therefore miss per-joker counters; they live in the table either way.

## state_events.h

**Counts**
- Structs: 0. `state_events.lua` defines no class, so there are no class objects and no "in dyn:" block.
- Prototypes: 16, in file order. Three are global functions (`win_game`, `end_round`, `new_round`) and 13 are `FUNCS_*`.
- File-level state: `extern num mult; extern num hand_chips;`.

**Design decisions**
- **`mult` / `hand_chips`:** these are undeclared Lua globals (lines 615-616), so they are real C globals with the Lua names. The event queued at line 1031 reads them when it runs, not when it is queued; `state_events.c` must define them.
- **`FUNCS_draw_from_deck_to_hand(Val e)` returns `bool`:** `e` is a card count, not a UI element (`blind.lua:389` passes 1; `NIL` gives the default). It returns true only when it sets GAME_OVER, which `game.lua:3222` tests.
- **`FUNCS_discard_cards_from_highlighted(UIElement *e, bool hook)`:** `blind.lua:482` calls it as `(NULL, true)`. It is also a button referenced by string, so `state_events.c` must register it through a static one-argument wrapper that passes `hook = false`. In the `pre_discard` context, set the key `hook` only when it is true.
- **`FUNCS_get_poker_hand_info`:** returns `text`; the other four Lua returns are out-pointers in Lua order (`loc_disp_text`, `poker_hands`, `scoring_hand`, `disp_text`), each allowed to be NULL. `scoring_hand` is the same table as `poker_hands[text][1]`, and `evaluate_play` then mutates it in place (Splash, Stone cards, sort), as in the Lua.
- **Registration:** only `play_cards_from_highlighted` and `discard_cards_from_highlighted` are referenced by string (`config.button`, `button_listen`). The other `FUNCS_*` are only called directly.
- **Direct-call signatures:** `FUNCS_evaluate_round(void)`, `FUNCS_tutorial_controller(void)`, `FUNCS_tutorial_part(const char *_part)`, `FUNCS_draw_from_play_to_hand(Table *cards)`. The remaining `draw_from_*` functions and `evaluate_play` keep `(UIElement *e)`, unused, and callers pass NULL.
- **Card fields this file touches:** per `card.h`, `shattered`, `destroyed` and `lucky_trigger` are in `dyn`; `debuff` and `sticker_run` are members. Line 30's `v.sticker_run = nil` becomes `sticker_run = NULL`.

**Open points when the header was written**
- **`funcs_register`:** no header existing yet declares it, so the wrapper requirement for discard is documented in the header comment only.
- **Other readers of `mult`:** `hand_chips` is used only in `state_events.lua` (plus `Blind:modify_hand` parameters). The global `mult` was not searched for separately; it is exported either way.
- **`FUNCS_draw_from_play_to_hand`:** it has no caller in 1.0.1o, so `Table *cards` (array of `O(Card)`) comes from the body alone.

## common_events.h

**Counts:** 0 structs (the Lua defines no class), 0 externs (no file-level locals), 57 prototypes. That is the 55 global functions of `common_events.lua` in file order (a script confirmed none is missing) plus `ease_value_f` and `ease_value_n`.

**Scope:** `evaluate_poker_hand`, `get_flush`, `get_straight`, `get_X_same`, `get_highest`, `find_joker`, `pseudorandom*`, `localize`, `attention_text`, `play_sound`, `mix_colours` and friends are in `functions/misc_functions.lua`, not this file. They are not declared here; the header says so and points to `misc_functions.h`.

**Design decisions**
- **`ease_value` is three functions**, following `event.h`'s ref forms:
  - `ease_value(Table *, const char *key, ...)` for a table or an object's dyn;
  - `ease_value_f(float *, ...)` for a struct float or a colour channel (Lua `colour[4]` is `&colour[3]`);
  - `ease_value_n(num *, ...)` for a num member.
- **`not_blockable` is an `int`**: -1 for Lua nil, 1 for true, 0 for false. The Lua computes `blockable = (not_blockable == false)`, so a Lua `nil` must be written -1, never 0.
- **`ease_type` is kept but has no effect.** The Lua passes it as `type`, which Event never reads (`event.h` says the same).
- **Float arguments marked "< 0: nil"** take any negative as nil and the callee applies the default; 0 is a real value. This covers the delays, `percent`, `vol`, `card_scale`, `poll_edition`'s `_mod` and `delay()`'s `time`.
- **`card_eval_status_text` with `percent < 0`** is the only case that calls `math.random()`, as in the Lua.
- **`Val` is used where nil must differ from a legitimate 0 or negative:**
  - `_rarity` in `create_card` / `get_current_pool`: NIL consumes a pseudorandom call, 0 does not.
  - `amount` in `level_up_hand`: it can be -1.
  - `playing_card` in `copy_card`.
  - `eval_func` in `juice_card_until`: a `FN()` called with `O(card)`.
- **`level_up_hand(void *card, ...)`**: callers pass a Card, an AnimatedSprite (`blind.lua:555`), a Tag (`tag.lua:197`) or nil. A Tag is not a Moveable, so the source must branch with `is_a(card, &Tag_class)` and call `Tag_juice_up`; the others use `CALL(card, juice_up, ...)`.
- **`get_current_pool`** returns the pool and gives the seed key through `const char **_pool_key`. The pool is the same scratch table on every call (Lua's `G.ARGS.TEMP_POOL`), so it must be used before the next call.
- **Centers and prototypes are `Table *`**; colours are `float *`, and `P(float *)` inside tables (`ease_background_colour` args, `extra.colour`, `create_playing_card` colours).
- **`get_type_colour` returns `float *`**, which may be a `G.C` colour, a fresh one, or the constant `{0,1,1,1}`; callers must not write through it.
- **`ease_background_colour_blind(int state, ...)`** takes `int`, not an enum, because `game.h` was not there yet and `G.GAME.PACK_INTERRUPT` comes from a table.
- **`ease_chips(mod)`** takes the target value, not a delta, as in the Lua.
- **Two functions write their argument table:** `create_playing_card` writes `card_init`, and `tutorial_info` writes `args.highlight`.
- **`inc_steam_stat`** is declared because `button_callbacks.lua:2537` calls it; its body is a comment (Steam). The Steam branches of `fetch_achievements` / `unlock_achievement` and the debug values `G.FORCE_TAG` / `G.FORCE_BOSS` are marked not translated.
- **Local closures** (`_mod` in `ease_dollars`, `ease_discard`, `ease_hands_played`, and all event funcs) are statics of the `.c`, not in the header.

**Open points when the header was written**
- **Game fields:** `game.h`, `ui.h`, `cardarea.h`, `tag.h` and `misc_functions.h` did not exist, so whether these are Game members or dyn was open:
  - `G.ARGS.eased_cursor_pos` (shared with `game.lua:2937`);
  - `G.ACHIEVEMENTS`, `G.achievement_notification`, `G.collection_alert`, the eleven `G.your_collection_*_alert` boxes;
  - `G.OVERLAY_TUTORIAL` and its fields (`step`, `step_complete`, `Jimbo`, `content`, `highlights`, `button_listen`, `skip_steps`);
  - `G.playing_card`, `G.VIBRATION`, `G.CURR_VIBRATION`, `G.JIGGLE_VIBRATION`, `G.REFRESH_ALERTS`, `G.TAROT_INTERRUPT_PULSE`.
- **`G.C.BACKGROUND`:** whether it is a struct or a Table decides how `ease_background_colour` iterates it and which `ease_value` variant it uses for `contrast`.
- **`Tag_juice_up`:** its exact signature, and whether `Tag_class` fills the `juice_up` class slot.
- **`delay` name clash:** the global `delay(float, const char *)` keeps its Lua name. It does not clash with glibc; the PS2SDK headers were not checked.

## ui.h

**Counts:** 6 structs (`UIBox`, `UIElement`, `DynaText`, plus `UIPixellatedRect`, `DynaLetter`, `DynaString`), the `UIType` enum (`UIT_T`..`UIT_I`, `UIT_padding`), and 47 prototypes in file order (UIBox 14, UIElement 23, `is_UI_containter`, DynaText 9). There is one extra prototype, `ui_ref_get`, and four static inlines: `UI_colour`, `UIBox_new`, `DynaText_new`, `DynaText_focused`. `extern Class UIBox_class, UIElement_class, DynaText_class`; the two Lua files have no other file-level state.

**Design decisions**

- **Config stays a Table.** A UIElement's config is the definition's own table in `Node.config`, shared and mutated, not copied. Children are in `Node.children` (1..n plus keyed `h_popup`/`alert`), and the parent is `Node.parent`. Definition `nodes` arrays have nil holes, so walk them with `titer`, not a `for` to `tlen`.
- **Value encodings inside config** (documented at the top of the header):
  - colours are `P(float *)`, as node.h and card.h do; read them with `UI_colour(config, key)`;
  - `object` and `button_UIE` are `O(...)`;
  - `text_drawable` is `P(Text *)`;
  - `prev_value` is a raw `Val`, compared with `veq`.
- **Caches (`self->cfg.*`).** Only keys that no Lua file assigns after creation are cached (checked by grepping `config.KEY =` across the whole game).
  - UIElement: `draw_layer`, `force_collision`, `button_dist`, `vert`, `shadow_colour`, `r`, `emboss`, `focus_with_object`, `line_emboss`, `ext_up`, `res`, `padding`, `h`, `maxw`, `maxh`, `minw`, `no_recalc`, `ref_value`, `progress_bar`, `role`, `no_role`.
  - DynaText: `spacing`, `min_cycle_time`, `shadow_colour`, `reset_pop_in`, `non_recalc`, `offset_y`, `random_element`, `pitch_shift`, `pop_delay`, `rotate`, `float_`, `bump`, `maxw`.
  - Not cached, on purpose, because callbacks or ui.lua write them: `w`, `minh`, `scale`, `text`, `colour`, `outline_colour`, `outline`, `shadow`, `hover`, `chosen`, `draw_after`, `force_focus`, `id`, `object`, `lang`, `button*`, `func`, `ref_table`; and for DynaText `pop_in`, `pop_out`, `pulse`, `quiver`, `W`, `H`, `string`. So `h` is cached but `w` is not, and `minw` but not `minh`.
  - The caches are filled by `UIElement_init` and at the start of `DynaText_init`.
- **`ui_object_updated` and `non_recalc` live in dyn for every class, DynaText included.** `UIElement_update_object` reads them on any object, and the Jimbo code sets them on a Card_Character.
- **`ui_ref_get(ref_table, ref_value)`** is declared here and implemented in ui.c. It exists because `ref_table` is sometimes an object whose field is a struct member, not a table: Card `cost`/`sell_cost_label` (card.h already has `Card_field`), Blind `chip_text`/`loc_name`, DynaText `focused_string`, G `setup_seed`/`run_setup_seed`.
- **Duck typing.** `v.draw_self`, `v.set_wh`, `v.align` and the like mean `is_a(v, &UIElement_class)`; `object.recalculate` and `object.get_UIE_by_ID` mean `is_a(object, &UIBox_class)`. No class extends these three.
- **`draw_self` is not a Class entry for UIElement.** That slot has the sprites' `(self, overlay)` signature; call `UIElement_draw_self` and `UIElement_draw_children` by name.
- **"self.T is nil"** (UIElement before `set_values`, DynaText inside its init) is `self->moveable.node.T == NULL`.
- **Signatures that differ from what you might guess:**
  - `UIBox_get_UIE_by_ID(self, Val id, Node *node)`: ids are strings or numbers, and the walk crosses keyed UIBox children, hence `Val` and `Node *`.
  - `UIBox_get_group` and `UIBox_remove_group` take `Val group`.
  - `UIBox_calculate_xywh` returns `bool` (false where the Lua returns a nil w or h) with `*w`, `*h` out; `_T` is a `const Transform *`; `_scale < 0` means nil.
  - `UIElement_draw_pixellated_rect` takes `Val _emboss, Val _progress`, because nil and 0 differ.
  - `UIElement_set_wh` returns w, with h as an out-pointer.
  - `UIBox_recalculate` and `UIElement_initialize_VT` take no argument (the Lua ignores what callers pass).
- **Nil-able numbers.** `last_clicked`, `focus_timer` and `object_focus_timer` have `has_*` flags; time stamps are `num`.
- **`pixellated_rect`** is a lazily allocated pointer (about 1.2 KB each), freed in `remove`.
- **DynaText storage.** `strings` and `letters` are C arrays, 0-based storage for 1-based Lua indices; `focused_string` stays 1-based. `start_pop_in` is a bool of Lua truthiness (`N(0)` counts). Call `Text_release` on the letters' `Text *` when they are rebuilt.
- **`UIBox_new` / `DynaText_new`** exist because definitions construct these inline inside table expressions.

**Open points when the header was written**

- What game.h makes of `G.LANG` and the font table. `config.lang` and `DynaText.font` are left as tables (`Table *font`, read as `font.FONT`, `FONTSCALE`, `TEXT_OFFSET`, `squish`, `TEXT_HEIGHT_SCALE`). If game.h makes fonts a struct, that member type must change.
- The name of the `G.FUNCS` lookup-by-name call (`funcs_register`'s counterpart) and the form of `G.I[instance_type]`. ui.c needs both from button_callbacks.h and game.h.
- Whether Blind, Game and other object `ref_table` sources expose a `<Class>_field` accessor like `Card_field`. `ui_ref_get` must dispatch on whatever those headers provide.
- A B or O node with no size: the Lua stores nil in `T.w`/`T.h` on recalculate, which a `Transform` cannot hold. No single-line B definition in the game lacks `w` and `h` (multi-line definitions were not checked), so this is noted in the header and not modelled.
- `UIConfig`/`UIDef` structs were considered and dropped: the config has to stay a Table, since it is shared and mutated.

## misc_functions.h

**Counts**
- Structs: 1 (`Utf8Iter`, the iterator for `utf8.chars`), plus the `Utf8Fn` callback typedef. The Lua defines no class, so there are no class structs and no `extern Class`.
- Prototypes: 94, covering 90 of the file's 97 global functions in file order, plus `utf8_map`, `utf8_chars`, `utf8_next` and a `localize_s` shorthand. `copy_table` and `localize_s` are static inline.

**Design decisions**
- **Sound functions not declared here.** `PLAY_SOUND`, `STOP_AUDIO`, `SET_SFX`, `MODULATE`, `RESTART_MUSIC`, `AMBIENT`, `RESET_STATES` (lua 1892-2022) are already declared in `io.h` and defined in `sound_manager.c`. `misc_functions.c` must not define them; `play_sound` and `modulate_sound` call `sound_manager_request`.
- **Random functions.** Everything is `num`.
  - `pseudorandom(Val seed, Val min, Val max)`: seed is `S(key)` or `N(seed)`; `NIL` min/max means `math.random()`.
  - `pseudorandom_element(Table *, Val seed, Val *key)`: returns the element, key through the out-pointer; `NIL` seed means no reseed; an empty table is `ERROR`.
  - `pseudoshuffle(Table *list, Val seed)` works on a `Table *`.
  - `pseudoseed(key, predict_seed)`: `NULL` is nil; the `%.13f` text round trip is required.
- **Colours.** A colour is `float *`; inside a Table it is `P(float *)`.
  - `HEX`, `mix_colours`, `lighten`, `darken`, `adjust_alpha` allocate 4 floats that are never freed.
  - The Lua's `no_tab` flag became a `float *` destination: `NULL` allocates, otherwise it writes there and returns it.
  - `mix_colours` has an extra `out` argument that is not in the Lua, so per-frame drawing code (ui.lua:788, 827) does not leak.
- **`localize(Val args, const char *misc_cat)` returns `Val`.** The header comment lists the result per `type`. `localize_s(key, cat)` is the shorthand when a string is known.
- **`play_sound(code, float per, float vol)`**: a negative value means nil (default 1); 0 volume is a real value.
- **`prep_draw(void *moveable, float scale, float rotate, const Vec2 *offset)`** and **`add_to_drawhash(void *obj)`** take `void *` so subclasses need no cast. `rotate` 0 stands for nil (same picture).
- **`Vector_Dist(Vec2, Vec2)`** drops the `mid` argument: the only caller (controller.lua:342) passes cursor points and no `mid`, and `ControllerCursor.T` is a `Vec2`.
- **Parameter renames**: `get_X_same(num num_, ...)` and `number_format(Val _num)`, because `num` is a typedef.
- **Mixed returns**:
  - `get_joker_win_sticker` returns `Val` (a number with `index`, else a sticker name or `NIL`).
  - `get_deck_win_stake(key, num *_w_low)`: `NULL` key means all decks; the second result goes through the out-pointer.
  - `get_front_spriteinfo` returns `Atlas *` and the pos table through an out-pointer.
  - `get_chosen_triangle_from_rect` writes 6 floats.
- **`get_stake_sprite`, stake 8**: the Lua overrides the instance's `draw`. The .c needs a private `Class` whose super is `Sprite_class`.
- **Declared but empty by design**: `nuGC` (no collector), `send_score` / `send_name` (HTTP, flags never set). `timer_checkpoint` only touches `G.PREV_GARB`.
- **`get_blind_amount`** with a scaling other than nil/1/2/3 returns nil in the Lua; documented as `ERROR`.

**Open points when the header was written**
- **Fields of G.** `game.h` did not exist yet. This file creates or assigns `PREV_GARB`, `check`, `LOADING`, `DRAW_HASH`, `E_SWITCH_POINT`, `SPLASH_VOL`, `PITCH_MOD`, `PROGRESS`, `DISCOVER_TALLIES`, `action`, `culled_table`, `FILE_HANDLER`, `SAVED_GAME`, `playing_card`, `C.STAKES` and `G.ARGS.{play_sound, push, score_intensity, ambient_sounds, save_run, LOC_COLOURS, bt}`. None of them is declared here; the header says they are Game members if `game.h` declares them, else in G's dyn table.
- **`save_run` and `pairs(G)`.** The Lua finds card areas by iterating G; in C this needs the list of `CardArea` fields from `game.h`.
- **`CardArea.cards`.** `cardarea.h` did not exist yet. `pseudoshuffle`, `find_joker`, `remove_all` and `evaluate_poker_hand` assume card lists are `Table *` arrays of `O(Card)`. If `cardarea.h` uses a C array, `CardArea_shuffle` and the callers need an adapter.
- **Colours in loaded data.** `data.h` loads `G.C` and `boss_colour` as `{r,g,b,a}` tables. The header assumes `game.h`'s loader turns them into `P(float *)`, as `card.h` does; `get_blind_main_colour` and `loc_colour` depend on it.
- **UI node tables in `localize`.** Their shape (`n`, `config`, `nodes`, and how `G.UIT.x` is stored) is `ui.h`'s.
- **`math.clamp` and similar.** The game defines no Lua-library extension anywhere except `utf8.map` / `utf8.chars`, so there is nothing to declare.

## cardarea.h, blind.h, tag.h, back.h, card_character.h, challenges.h

| Header | Structs | Prototypes | Class objects | Inline accessors |
|---|---|---|---|---|
| `cardarea.h` | 1 (`CardArea`) | 23 | `CardArea_class` | 5 |
| `blind.h` | 1 (`Blind`) | 25 | `Blind_class` | 3 |
| `tag.h` | 1 (`Tag`) | 14 | `Tag_class`, `Tag_sprite_class` | 0 |
| `back.h` | 1 (`Back`) | 8 | `Back_class` | 0 |
| `card_character.h` | 1 (`Card_Character`) | 11 | `Card_Character_class`, `Card_Character_card_class` | 1 |
| `challenges.h` | 0 | 0 | none | 0 |

Total: 5 structs, 81 prototypes, 7 class objects. `challenges.lua` has no code, so its header only documents the shape of a `G.CHALLENGES` entry.

**Design decisions**

- **CardArea storage:** `cards` and `highlighted` are `Table *` arrays of `O(Card *)`, 1-based; use `CardArea_card(a, i)` and `CardArea_n(a)`. `cards` is NULL only after `CardArea_remove`. `config` is the Node's config table, not a struct.
- **CardArea draw layers:** `self.ARGS.draw_layers` became the member `draw_layers`, computed once on first draw and kept, as in the Lua.
- **Nil floats:** `CardArea_hard_set_T`, `Blind_change_dim` and the `yoff` of `Card_Character_add_button` take NAN as nil, matching `card.h`. Sizes and scales (`Tag_generate_UI`, `Back_generate_UI`, `juice_up`) take `< 0` as nil.
- **`CardArea_draw_card_from`:** the `stay_flipped` argument is ignored, because the Lua shadows it.
- **Blind shared states:** the Lua makes the sprite's `states` the blind's own table, but `node.h` holds `States` by value. The sources must read and write the blind's states wherever the Lua names the sprite's, and call the inline `Blind_sync_states()` after writes and in `Blind_move`.
- **Blind and the sprite shader:** `tilt_var`, `ambient_tilt`, `hover_tilt`, `dissolve` and `dissolve_colours` are named and typed as in `card.h`, because `Sprite_draw_shader` reads them from its draw major. The defeat ease uses `.ref_float = &blind->dissolve`.
- **Blind UI bindings:** `Blind_field(blind, key)` serves `ref_table = G.GAME.blind` (`chip_text`, `loc_name`), like `Card_field`. `loc_debuff_lines` stays a `Table *` that is never replaced, because the HUD binds to it by index.
- **Blind nil handling:** `discards_sub` and `hands_sub` are `Val` (NIL or a number). `only_hand` is NULL for both false and nil, and `sound_pings` is 0 for nil.
- **Several returns:**
  - `Blind_modify_hand` returns mult and writes `*out_hand_chips` and `*modded`.
  - `Back_trigger_effect` returns `Val` chips (NIL unless Plasma at `final_scoring_step`) and writes `Val *mult`.
  - `Tag_generate_UI` returns the definition and writes `Sprite **tag_sprite`.
  - `Tag_apply_to_run` returns `Val`, because the type really varies (NIL, true, a table for `eval`, a Card for `store_joker_create`).
- **Per-instance method overrides** are done with dedicated classes whose `cls` is swapped in after init:
  - `Tag_sprite_class` (super `Sprite_class`) carries `Tag_sprite_hover` and `Tag_sprite_stop_hover`; the tag is read back from the sprite's `config.tag`.
  - `Card_Character_card_class` (super `Card_class`) has `hover = Node_hover`.
  - The sources must call `class_init` on these before first use if `classes_init` does not cover them.
- **Fields that live in another object's dyn:**
  - On the tag's sprite: `float`, `hovering`, `hover_tilt`, `ability_UIBox_table`.
  - On Jimbo's card: `jimbo`.
- **Tag and Back are `Object` subclasses**, not Nodes. `Tag_class` must fill the `juice_up` and `remove` slots, because `level_up_hand` is given a Tag as its card.
- **Tag controller locks** use a numeric key: `tseti(G->CONTROLLER->locks, self->ID, ...)`.
- **`Tag_yep`** takes `bool (*func)(void *ctx), void *ctx`, the same shape as `EventFunc`, without including `event.h`.
- **`G.I.CARD` is mixed:** it also holds the Blind and Card_Characters, so loops over it need `is_a` before treating an entry as a Card.
- **Dropped Lua oddities:** the accidental global `loading` (`cardarea.lua:642`) is read by nothing and does not exist in C. `Card_Character:draw(dt)` loses its unused `dt`. `Card_Character.highlight` is kept as an always-NULL member.

**Open points when the header was written**

- **UI ref resolution:** `ui.h` did not exist yet, so how it resolves `ref_table = <object>` was open. The headers assume the `Card_field` pattern from `card.h`.
- **Sprite draw major:** `sprite.c` was not written, so how it reads `hover_tilt`, `dissolve` and the rest from a draw major that may be a Card, a Blind or a plain Sprite was open. The header assumes `is_a` dispatch on the members, and dyn for a plain Sprite.
- **Game member names:** `game.h` did not exist yet, so `G->hand`, `G->HUD_tags`, `G->tagid`, `G->CHALLENGES` and similar are assumed.
- **Class registration:** who registers the two extra classes in `classes_init` (it is `object.c`: see `docs/source-notes.md`).

## button_callbacks.h

**Counts:** 0 structs (the Lua defines no class), 2 typedefs (`FuncsFn`, `FuncsArgsFn`) and 176 prototypes:
- 165 `FUNCS_*`, one for every `G.FUNCS.*` of the file, in file order.
- 1 function-value adapter, `FUNCS_refresh_contrast_mode_fn`.
- 5 Lua globals: `GET_TEXT_FROM_INPUT`, `MODIFY_TEXT_INPUT`, `TRANSPOSE_TEXT_INPUT`, `INIT_COLLECTION_CARD_ALERTS`, `scale_number`.
- 5 registry functions: `funcs_register`, `funcs_register_args`, `funcs_lookup`, `funcs_lookup_args`, `funcs_register_all`.

**Design decisions**

1. **Two registries, not one.** The Lua calls `G.FUNCS[name]` in two shapes, and no name is used in both.
   - `config.button` / `config.func` get the element: `funcs_lookup(name)(e)`, type `void (*)(UIElement *)`.
   - `opt_callback` of option cycles (`:572`) and `callback` of sliders (`:508`) get a table: `funcs_lookup_args(name)(args)`, type `void (*)(Table *)`.
   - Lookup returns NULL when the name is absent; registering again replaces.

2. **Each prototype's comment carries a registration tag.**
   - No tag: registered directly.
   - `[args]`: registered in the args table.
   - `[wrap: ...]`: `funcs_register_all` registers a static wrapper `(UIElement *e)` that calls it with the arguments shown.
   - `[direct]`: not registered, only called by name from C.
   - `funcs_register_all` and the registry are to be implemented in the last part of `src/button_callbacks.c`.

3. **Signatures that deviate from `(UIElement *e)`:**
   - `FUNCS_use_card(UIElement *e, Card *card, bool mute, bool nosave)`: `tag.lua:216-276` and `game.lua:2205` pass a made-up `{config = {ref_table = card}}`. In C that call is `FUNCS_use_card(NULL, card, false, false)`; the button is `(e, NULL, false, false)`. Writes to `e` are skipped when it is NULL.
   - `bool FUNCS_can_continue(UIElement *e)`: returns a value; `e` NULL stands for the made-up `{config = {func = true}}` of `UI_definitions.lua:5302`.
   - `FUNCS_change_challenge_description(NULL)` stands for the made-up `{config = {id = 'nil'}}` of `:1647`.
   - `FUNCS_load_profile(bool)`: its button wrapper passes `true`, because the Lua receives the element as `delete_prof_data`, which is truthy. Pressing "load profile" really does reset the in-memory profile before reloading it.
   - `FUNCS_apply_window_changes(bool _initial)`: its wrapper passes `false`.
   - `(void)`: `exit_overlay_menu`, `continue_unlock` (both wrapped), `change_window_cycle_UI`, `refresh_contrast_mode`, `wipe_off`.
   - Extra arguments: `current_hands(e, bool)`, `settings(e, bool)`, `end_consumeable(e, float delayfac)`, `start_run(e, Table *args)`, `slider_descreet(e, num per)`, `text_super_juice(e, float)`.
   - Other types: `overlay_menu(Table *)`, `text_input_key(Table *)` (it writes into `args`), `test_framework(Table *)`, `bool check_for_buy_space(Card *)`, `wipe_on(Table *message, bool no_card, float timefac, const float *alt_colour)`.
   - `FUNCS_buy_from_shop` is `void`; the Lua's `return false` is never read.

4. **Nil defaults:** `delayfac < 0`, `timefac < 0` and `max < 0` (in `scale_number`) mean nil and take the Lua default. `scale_number` takes `number` as `Val` because the Lua type-checks it.

5. **`G.FUNCS.refresh_contrast_mode` as a value:** `UI_definitions.lua:2307, 2578` pass the function itself as a toggle callback. Use `FN(FUNCS_refresh_contrast_mode_fn, NULL)`.

6. **G.FUNCS of other files** (listed in the header comment):
   - `state_events.lua`: `play_cards_from_highlighted` (direct) and `discard_cards_from_highlighted` (wrapped with `false`) are registered by `funcs_register_all`. `state_events.h` says its own `.c` registers them too, which is harmless. The other eleven are `[direct]`.
   - `UI_definitions.lua`: the 16 `DT_*` are debug-only and not translated; `false_ret` (`:5303`) is never referenced and not registered.
   - `game.lua` sandbox (`col1change`, `col2change`, `edition_change`, `pulseme`, `spawn_joker`, `rem_joker`, `do_time`, `cb`): not registered here. If `game.c` translates `Game:sandbox`, it must register them itself at that point, as the Lua assigns them at run time.

**Open points when the header was written** (`ui.h` and `game.h` did not exist yet)

- Where the G fields this file uses live (`G.OVERLAY_MENU`, `G.screenwipe`, `G.SAVED_GAME`, `G.focused_profile`, `G.viewed_stake`, `G.TAROT_INTERRUPT` and others): member of `Game` or `G->dyn`. The header lists them and defers to `game.h`.
- How a colour is stored in a UI config table (`e.config.colour`, and `colour[4]` written in `FUNCS_flash`): deferred to `ui.h`.
- How `alignment.offset.py` is held: `Moveable.alignment.offset` is a `Vec2` with no `py`, so the header says it lives in the UIBox's dyn, nil when not saved. `G.screenwipe.colours` is in dyn too; `wipe_off` eases its alpha through an Event `ref_table` / `ref_value = 4`, which depends on how `event.h` models eased references to a colour.
- `e.disable_button` (`:2165`, `:2409`) is a field on the element, not its config: dyn unless `ui.h` declares it.
- Whether `love.h` offers `love.window.updateMode`, `love.system.openURL`, the clipboard and `love.event.quit`, which this file calls; the bodies need them or a `TODO_PORT`.

## game.h

Every name in `tools/G_fields.txt` (every `G.<name>` the game's Lua uses) appears in the header.

**Counts**
- 1 class struct (`struct Game`, `Object` first, about 300 members, with nested `TIMERS`, `FRAMES`, `exp_times`, `I`, `C`, `WINDOWTRANS`, `window_prev`, `ROOM_ORIG`).
- 2 enums: `GameState` (`STATES_*`) and `GameStage` (`STAGES_*`).
- 56 prototypes: 39 from game.lua (37 methods plus the nested globals `create_UIBox_sandbox_controls` and `make_splash_card`), `Game_set_globals`, and 16 for main.lua.
- 14 static inline accessors, `extern Class Game_class`, and the macros `RELEASE_MODE`, `DEMO_MODE`, `OVERLAY_MENU_TRUE`, `G_CARDAREA_COUNT` / `G_CARDAREA_NAMES`.

**Design decisions**
1. **Two G fields are not members.** `setup_seed` and `run_setup_seed` live in `G->object.dyn`, because the UI binds them with `ref_table = G` and object bindings go through dyn (`ui.h`'s `ui_ref_get` already names this case). Read them with `G_setup_seed()` / `G_run_setup_seed()`. This deviates from "every field a member"; writing `G->setup_seed` will not compile.
2. **Other dyn fields.** The ten `your_collection_*_alert` boxes (`G[v.alert_uibox_name]`) and any `load_<k>` other than the three shop ones are also in dyn.
3. **Data stays in Tables.** `GAME`, `SETTINGS`, `PROFILES`, `P_*`, `LANGUAGES`, `FONTS`, `LANG`, `ARGS`, `FILE_HANDLER`, `hand_text_area`, `STAGE_OBJECTS` and similar are `Table *`, matching `data.h` and `ui.h`. A font's `FONT` is `P(Font *)`, read with `font_FONT(t)`. Atlases and shaders are tables of `P(Atlas *)` / `P(Shader *)`, read with `ASSET_ATLAS_get`, `ANIMATION_ATLAS_get`, `SHADERS_get`.
4. **Colours.** `G->C.X` is a `Colour` and decays to `float *`. Specifics:
   - `C.SUITS.*` are `float *` aliasing `SO_1` / `SO_2`, as in the Lua; `SUITS_init` holds the four literals.
   - `RARITY[5]` is 1-based and `HAND_LEVELS[8]` is 0..7, with `[0]` a copy of RED rather than RED itself.
   - `STAKES[9]` holds pointers and is NULL until `get_stake_col` fills it.
   - `UI_CHIPLICK` / `UI_MULTLICK` being all zero means the Lua's nil.
   - `G.C.UI.RED` does not exist in the Lua either, so it is NULL.
   - Name lookups are `C_SUITS_get`, `C_SO_get(n, suit)`, `C_SET_get`, `C_SECONDARY_SET_get`, `C_BLIND_get`. There is no generic `G.C[name]`; only the debug sandbox uses it.
5. **Timers are floats**; `G.TIMERS[name]` is `*TIMERS_get(name)`. Precision degrades after roughly 9 hours of uptime, noted in the header.
6. **References into G's structs.** In an Event use `.ref_float = &G->TIMERS.REAL`. In a Sprite draw-step send entry, replace `ref_table` + `ref_value` with `ref_float = P(&float)` or `ref_colour = P(colour)`. The header states the read order (val, ref_float, ref_colour, ref_table[ref_value]); `sprite.c` must honour it, since `node.h` is silent on this.
7. **`OVERLAY_MENU = true`.** The Lua sets this while the box is being built and code tests its truthiness, so it becomes `OVERLAY_MENU_TRUE` (`(UIBox *)1`): non-NULL, never dereferenced.
8. **Nil encodings.** Bools merge nil and false. Members marked "0 as nil" include `ID`, `sort_id`, `tagid`, `playing_card`, `TAROT_INTERRUPT`, `F_RUMBLE`, `FPS_CAP`, `E_SWITCH_POINT`, `viewed_stake`, `focused_profile`, `forced_stake`, `REFRESH_FRAME_MAJOR_CACHE`, `vortex_time`. `Val` is used where 0 differs from nil: `F_DISP_USERNAME`, `last_materialized`, `SPLASH_VOL`, `video_organ`, `prof`. `screenwipe_amt` has a companion `screenwipe_amt_set`.
9. **`your_collection` is a `Val`**: an array of card areas, or a single `O(CardArea *)` at UI_definitions.lua:3785 and :4144.
10. **Card areas by name.** `G[name]` is `*Game_cardarea(G, name)`. The Lua's `pairs(G)` scans for card areas (`delete_run`, `save_run`) use `G_CARDAREA_NAMES`, in creation order rather than Lua hash order.
11. **`STATE` and `STAGE` are `int`** (`STATE` is -1 after `delete_run`). The enum types are named `GameState` / `GameStage` because `node.h` already has `State` / `States`.
12. **Managers hold nothing.** `SOUND_MANAGER`, `SAVE_MANAGER`, `HTTP_MANAGER` are empty tables used only for truthiness; `channel:push` is `sound_manager_request` / `save_manager_request` from `io.h`.
13. **`G.ROOM.jiggle`** is in `G->ROOM->object.dyn`, since `Node` has no such member.
14. **main.lua.** `love.run` is `love_run()` plus `love_run_frame(int *exit_code)` for the closure it returns. The top-level chunk and `G = Game()` are `main()` in `src/main.c`, with no prototype. Joystick arguments are `Joystick *`.
15. **Sandbox callbacks** (`col1change` and the rest) are statics of game.c registered by name, with no prototypes; `button_callbacks.h` agrees.

**Open points when the header was written**
- Who defines `_RELEASE_MODE` / `_DEMO`: conf.lua has no header, so guarded macros `RELEASE_MODE` (true) and `DEMO_MODE` (false) are defined here. If another header defines them differently, that one must yield.
- `node.h` mentions `classes_init()` but nothing declares it; `main.c` needs it (it is declared in `include/needs/`).
- Member names `DEBUG`, `VERSION` and `I` break if the build passes `-DDEBUG` / `-DVERSION=` or anything includes `<complex.h>`.
- The types of debug-only fields are best guesses: `video` (`void *`), `prof` (`Val`), `check`, `LOADING`, `action`, `culled_table`, `debug_tool_config` (`Table *`).
- `Game_state_col` takes its state as `num`; its only caller is the untranslated debug overlay.

## UI_definitions.h

**Counts**
- Structs: 0 (the Lua file defines no class).
- Prototypes: 120. That is the 116 distinct functions of the file (117 `function` definitions; `create_UIBox_highlight` is defined twice, identically, at :463 and :6302), plus `FUNCS_false_ret`, `UIDEF_card_focus_ui_set_alignment`, `UIDEF_blind_sprite_hover` and `UIDEF_blind_sprite_stop_hover`.
- Externs: `UI_global_type`, `UI_global_set`, `saved_game`, `UIDEF_blind_sprite_class`.

The credits body (:4344–5196) was not read line by line for the header; it is static text plus the closure that creates `G.collab_credits`.

**Design decisions**
- **Functions stored as values** are `V_FN`; the wrappers are `static` in whichever `.c` needs them. The header's top comment fixes the calling contract for each:
  - `tab_definition_function`: one argument (`tab_definition_function_args`, NIL when absent), returns `T(root)`.
  - `filler.func`: one argument (`filler.args`).
  - `create_toggle`'s `callback`: one argument (the new value). `G.FUNCS.refresh_contrast_mode` is stored there as a function, not by name.
  - `create_text_input`'s `callback`: no arguments.
- **Replaced `hover` / `stop_hover` on blind chips** (:4003 collection, :5834 challenge restrictions) become one class, `UIDEF_blind_sprite_class` (super `AnimatedSprite_class`), following the `Tag_sprite_class` pattern.
  - The class object and its two functions are defined in the `.c` that holds `create_UIBox_your_collection_blinds`; the challenge-tab `.c` only uses them.
  - Upvalues are read back from the sprite: `v` is `config.blind`, `discovered` is a bool in the sprite's dyn (`true` for the challenge tab).
  - One hover serves both closures because the challenge sprites never get `children.alert`.
- **`base_background.set_alignment` override** (:325) is the plain function `UIDEF_card_focus_ui_set_alignment(Card *)`; `Card_align` (card.lua:4110) should call it.
- **Accidental Lua globals** are file-level state:
  - `_type` / `_set` are assigned in `create_UIBox_usage` and `_type` is read by `UIDEF_viewed_stake_option`; they are named `UI_global_type` / `UI_global_set` to stay out of C's reserved underscore names.
  - `saved_game` is assigned and read in `UIDEF_run_setup_option`.
  - `disabled` (in `create_UIBox_blind_tag`), `badges`, `maxw` and `show_win_cta` are never assigned anywhere, so they are always nil.
- **Arguments typed `Val` because the type really varies**:
  - `UIDEF_speech_bubble` `text_key`: a key or a table of lines.
  - `name_from_rows` `name_nodes`.
  - `tally_sprite` `value`.
  - `UIDEF_challenge_description` `_id`: an index or the string `'nil'`.
  - `desc_from_rows` `maxw`: NIL as nil.
- **`UIDEF_card_h_popup` takes `Node *`**, because it is also called with a tag's Sprite (tag.lua:525, button_callbacks.lua:2724), not only a Card.
- **`UIDEF_run_setup` takes `const char *from_game_over`** (the opening button's id, NULL as false).
- **Unused Lua parameters dropped** where no caller passes them: `UIDEF_deck_preview(void)`, `UIDEF_viewed_collab_option(void)`. `create_shop_card_ui` keeps its unused `type` and `area` because callers pass them.
- **Widgets write their defaults into `args`** and keep `args` as `ref_table`, so the C must mutate the caller's table too. `attention_text` and `create_UIBox_generic_options` (with `infotip`) queue events that read `args` later.
- **Not translated (bodies are `TODO_PORT`)**: `create_UIBox_debug_tools` with its `G.FUNCS.DT_*`, `create_UIBox_online_high_scores`, `create_UIBox_high_scores_filling`.

**Open points when the header was written**
- **button_callbacks.h**: this file calls three `G.FUNCS` entries with plain tables instead of a `UIElement`, and uses one return value. The `void FUNCS_x(UIElement *e)` convention does not cover these:
  - `can_continue({config = {func = true}})`, return value used (:5302).
  - `change_stake({to_key = ...})` (:5992).
  - `change_challenge_list_page{cycle_config = ...}` (:5509).
- **game.h**: where the `G` fields this file creates live (listed in the header's top comment). `G.your_collection` is an array of CardAreas in most collection screens but a single CardArea in `create_UIBox_your_collection_seals` and `create_UIBox_card_unlock`, so it needs a `Val` or table slot.
- **`FUNCS_false_ret`** returns `false` in Lua; it is declared `void` per convention, and nothing in the game refers to it.
- **`create_UIBox_demo_video_CTA`** uses `love.graphics.newVideo`, which love.h does not offer (demo-only path).
- **`G.F_DAILIES`** is never set anywhere in the Lua, so the `UIDEF_daily_overview` branch of `UIDEF_challenges` looks dead; the header comment says "off" on that basis.
