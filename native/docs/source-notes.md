# Source notes

Per-file notes on src/: where the C had to depart from a literal reading of the Lua it mirrors, and what each file
assumes of the others. One section per source file; a file named `x_N.c` is the N-th part of `x.lua`.

They were written with each file, before the whole was linked and run: what was found and fixed afterwards is in
`docs/development-log.md`. The rules they refer to are in `docs/conventions.md`, the headers' own in
`docs/header-notes.md`.

## blind_2.c

Functions: Blind_debuff_card, Blind_move, Blind_change_dim, Blind_align, Blind_save, Blind_load.

Notes:
- Blind_save booleans: blind.h keeps disabled/boss/triggered as "nil as false", so nil cannot be told from false. `disabled` is always written (as true/false); `boss` and `triggered` are written only when true. Blind_load reads all three by truthiness, so a round trip is unaffected, but the saved table can differ from the Lua's in whether a false key is present.
- Blind_save numbers: dollars, mult and chips are always written (they are plain num members, so a Lua nil before the first set_blind comes out as 0).
- Shared states: wherever the Lua names the sprite's states (blind.lua:670, 734, 740, 743) the C reads or writes the blind's own states and calls Blind_sync_states. Blind_move also calls Blind_sync_states first, before Moveable_move and Blind_align, as blind.h asks.
- Blind_align: the sines are computed in double from the float G->TIMERS.REAL and stored into the float transforms. Every child other than 'animatedSprite' is treated as a Node (only its T is touched).
- Blind_load: the sprite's set_sprite_pos, juice_up(0.3, nil) and hard_set_VT go through CALL (class dispatch). `dollars_to_be_earned` is str_rep(vstr(localize(S("$"), NULL)), (int)dollars). There are no NULL guards on G.ARGS.spin, the 'dollars_to_be_earned' HUD element or its config.object, matching the Lua, which would error there too.
- debuff.value and debuff.nominal are compared to card.base.value / card.base.nominal with veq.

## back.c

All 8 functions of back.h are implemented with the header's signatures.

Notes:
- back.c defines `Class Back_class = {.name = "Back", .super = &Object_class}` (back.lua:2). No virtual slots are filled; `class_init`/`classes_init` must cover it if they require every class.
- There are 6 static event functions besides the 8 header functions. `ev_deck_consumables` and `ev_deck_edition` take the Back itself as ctx and read `effect.config` when they run, as the Lua closure does.
- `loc_name` is set with `vstr(localize(...))`, so a multi-line name (a table) would give NULL. No Back has one.
- In `Back_generate_UI`, a challenge id that is not found relies on `get_challenge_int_from_id` returning 0, so `G.CHALLENGES[0]` is nil and the `{name='ERROR'}` fallback is used.
- The Challenge Deck button passes `id = T(challenge table)` to `UIBox_button`, as the Lua does, although UI_definitions.h documents id as S or N. With the ERROR fallback, `challenge.id` is nil, so `localize(NIL, 'challenge_names')` is the Lua's own error path.
- `min_dims*5` and `min_dims*2.5` are computed in float (geometry) and stored with `N()`.
- `Back_trigger_effect` sets `*mult` to NIL on every path except Plasma at `final_scoring_step`.
- The two `{0.8, 0.45, 0.85, 1}` literals passed to `ease_colour` are static const arrays. This assumes `ease_colour` only reads `new_colour`, which its `const float *` prototype suggests.
- The `pairs` loops over `config.vouchers` and `G.playing_cards` use `titer` (array order 1..n).
- Sound pitches such as `0.9 + math.random()*0.1` are computed in double and then narrowed to `play_sound`'s float argument.

## card_2.c

card.lua:645-1090 is translated in full (15 header functions, Card_remove_from_deck through Card_get_p_dollars).

Notes:
- card.lua:1048-1054 (Blue seal event): the Lua starts with `_planet = 0` and would pass that 0 as forced_key if no planet matched the last hand. create_card takes a `const char *`, so that case passes NULL (no forced key). It cannot happen with stock data.
- card.lua:676 (Astronomer event): `if v.set_cost` is translated as `is_a(v, &Card_class)`, because G.I.CARD also holds the Blind and Card_Characters.
- card.lua:666 (Oops! All 6s): G.GAME.probabilities is halved with tsetv on existing keys while iterating with titer. This assumes core.c allows assigning to existing keys during iteration.
- loc_vars holds raw Vals from the tables (tget), so nil entries stay nil and `ability.extra` keeps its type (number or table). The probability entries are strings via tostring (`''..n`), except Hallucination, which passes the raw number as the Lua does.
- Arithmetic on a nil field, which would raise an error in Lua, yields 0 here (vnum).
- UI nodes for Misprint, Blueprint/Brainstorm and Luchador are built inline with TAB/ARR: colours as P(float *), `ref_table = self` as O(self), `ref_table = self.ability` as T(self->ability), DynaText via DynaText_new stored as O().
- Badges are appended with tseti(badges, tlen+1, v). An edition with a nil type appends nothing, as in the Lua.
- lucky_trigger and vampired are read and written in the card's dyn table, as card.h specifies.
- Card_get_id uses math_random_range(100, 1000000) (unseeded stream) and returns 0 where base.id is nil.
- The three event closures capture nothing, so they have no ctx.

## card_3.c

All six functions of card.lua:1091-1680 are translated (Card_use_consumeable, Card_can_use_consumeable, Card_check_use, Card_sell_card, Card_can_sell_card, Card_calculate_dollar_bonus).

Notes:
- Closures: every event closure is a static ev_* function sharing one file-local context struct (Ctx). Identical bodies share one function: ev_tarot1_juice (lines 1102, 1180, 1223, 1275, 1294, 1348), ev_unhighlight_all (1150, 1190), and the two flip events, which take a from_cards flag for G.hand.cards vs G.hand.highlighted. Lists are looked up when the event runs; loop bounds are taken at call time, as in the Lua.
- Wheel / Ectoplasm / Hex: temp_pool captures the Table pointer of self->eligible_strength_jokers / eligible_editionless_jokers and reads it at fire time. This matches the Lua only if Card_update empties and refills the same table (tclear) and does not allocate a new one each frame.
- Card_can_use_consumeable, eligible arrays: a NULL eligible array gives false, where the Lua's next(nil) would be an error.
- Card_can_use_consumeable, mod_num: consumeable.mod_num is read from the table and a nil is an ERROR, as the Lua's comparison would be. It relies on Card_update writing mod_num into the shared center config.
- Black Hole: pairs(G.GAME.hands) runs in titer insertion order; only the order of the check_for_unlock events queued by level_up_hand can differ from the Lua's hash order.
- Ankh: _first_dissolve is a local of the single 'before' event that uses it, which is equivalent because that func runs once.
- juice_up is called as Card_juice_up directly (self, used_tarot and hand cards are Cards), not through CALL.
- G.TAROT_INTERRUPT_PULSE = nil is written as false; G.playing_card's "(x and x+1) or 1" is += 1, per game.h.
- children.use_button / sell_button are removed with CALL(box, remove) and the child key is set to NIL.
- G.GAME.blind is read as tobj(G->GAME, "blind") cast to Blind *, and its name member is compared with "Verdant Leaf".
- create_playing_card receives TAB("front", T(P_CARDS entry), "center", ...); this assumes tab_of accepts a NIL value if the P_CARDS key were missing.
- Sigil: a base.id outside 2..14 is an ERROR, as the Lua's concatenation of false would be. Strength formats an out-of-range rank as a number, as the Lua's concatenation does.

## blind_1.c

blind.lua:1-623 is translated in full.

Notes:
- Blind_class is DEFINED in blind_1.c (the Lua's line 2). It fills move, draw, hover, stop_hover and juice_up through static thunks; the rest is inherited from Moveable_class. It references Blind_move from part 2 (blind.lua:654), so blind_2.c must not define Blind_class again. Someone must call class_init(&Blind_class), or include it in classes_init.
- Blind_debuff_card (blind.lua:624, part 2) is called from Blind_set_blind and Blind_disable.
- Shared states: the sprite's states are read and written as the blind's, with Blind_sync_states called in Blind_init. Part 2's Blind_move must keep syncing, as blind.h says.
- G.GAME.last_blind.boss is written as NIL when blind is nil and B(self->boss) otherwise, because the Lua's self.boss is nil with no blind.
- self.sound_pings uses 0 as nil ("or 3"), per blind.h.
- Hash-order difference: self.hands (The Eye) is built in the Lua's source order, and the pairs(G.playing_cards) loop in Blind_disable uses titer (array order). Neither affects the rules.
- Lua crashes are not guarded: where the Lua would index nil (a HUD element missing from get_UIE_by_ID, G.play.cards[i] gone when The Tooth's event fires, G.buttons without 'play_button'), the C dereferences NULL.
- With reset = true before any set_blind, the Lua errors on config.blind.boss; the C reads it as nil and sets spin.real to 0.
- Event closures: the ones needing more than self use NEWCTX structs. The Tooth's inner event passes i through the ctx pointer as an intptr_t. The three Amber Acorn pitches are a static float array.
- The dissolve ease uses .ref_float = &self->dissolve; the blind lives for the whole run, so the pointer stays valid.
- Direct calls by name: AnimatedSprite juice_up and set_sprite_pos go through CALL (class dispatch); Sprite_draw_shader, DynaText_pop_in/pop_out, UIElement_juice_up, Card_flip, Card_juice_up and Card_set_debuff are called by name.

## card_1.c

card.lua:1-644 is translated: the 16 header functions from Card_init to Card_add_to_deck, plus the class object.

Notes:

1. **Card_class is defined here** (card.lua:2, non-static). Its slots are update, move, draw (calls Card_draw(self, NULL)), remove, hover, stop_hover, click, release, hard_set_T and juice_up, through static wrappers around the Card_* functions of the other card parts. No other card_N.c may define Card_class.

2. **Shared states are copied once, not shared.** The Lua makes the front, center and back sprites use the card's own states.hover / click / drag tables. States is by value in node.h, so Card_set_sprites copies them at sprite creation (static share_states). Whoever later writes a card's states.hover / click / drag (controller, Card_update, Card_move) must mirror them to those three children, or the sprites go stale.

3. **children.*.layered_parallax = nil (card.lua:68-70) is stored as {0, 0}.** A Vec2 cannot hold nil. prep_draw's fallback to the parent's parallax gives the same picture only while card areas keep a {0, 0} parallax, which is the case throughout the Lua.

4. **Dangling pointers in events.** The delayed set_sprites event (card.lua:262) reads self->REMOVED at fire time, and the edition and seal juice events keep the Card pointer. This is safe only while Card_remove does not free a card that still has events queued.

5. **`pairs` order for To Do List.** `pairs(G.GAME.hands)` is walked with titer (insertion order of the data table), where the Lua uses LuaJIT hash order. The hand picked matches the PC game only if G.GAME.hands is inserted in that order.

6. **The Astronomer event** (card.lua:617) tests `v.set_cost` as is_a(v, &Card_class), because G.I.CARD also holds the Blind and Card_Characters.

7. **params.playing_card** is read into the int member: a number is truncated, any other truthy value becomes 1, nil becomes 0.

8. **Lua errors made explicit.** Card_set_sprites raises ERROR if G.GAME[self.back] is nil when a playing card's back sprite is created, and Card_change_suit raises ERROR on an unknown suit; the Lua fails at the same spots.

9. **Stale floating_sprite.** As in the Lua, a previous children.floating_sprite is overwritten without being removed.

10. **Card_set_debuff takes a bool**, as the header fixes; the nil/false tri-state is collapsed.

## card_4.c

card.lua 1681-2290 is translated in full. The 10 functions are Card_open, Card_redeem, Card_apply_to_run, Card_explode, Card_shatter, Card_start_dissolve, Card_start_materialize, Card_calculate_seal, Card_calculate_rental and Card_calculate_perishable. Every closure and helper is static.

Notes:
- **Dissolve eases point into the card.** Every ease of `self.dissolve` uses `.ref_float = &self->dissolve`. In explode, shatter and start_dissolve the ease ends before the queued remove. In start_materialize it dangles if the card is removed within 1*dissolve_time by something else.
- **Explode's second particle ease points into the Particles.** The ease of `childParts2.scale` uses `.ref_float = &childParts2->scale` and runs from 1.4T to 1.5T, the same moment the card's remove fires. If Card_remove frees attached Particles, this is a use-after-free risk at the boundary.
- **Events keep raw `Card *` pointers.** All event contexts hold the card pointer (the Lua keeps the table alive). A card freed by another path before its events fire is a dangling pointer; this needs deferred free or a REMOVED check at the engine level.
- **Shared sprite states are not mirrored.** `self.states.hover.can` and `states.visible` are written on the card's own States only. If `sprite.c` mirrors the card's states onto front/center/back, these writes need the same sync.
- **Repricing loop filters by class.** The `pairs(G.I.CARD)` loop with `if v.set_cost` is `is_a(v, &Card_class)`, because G.I.CARD also holds the Blind and the Card_Characters.
- **Removal goes through the class.** `self:remove()` and `children.particles:remove()` use `CALL(obj, remove)`; the top_disp / bot_disp boxes use `UIBox_remove` by name.
- **Lua crashes became ERROR.** A booster whose name matches none of the five kinds (nil `card` at 1777), Reroll Surplus/Glut with a NULL self (1927), and a nil childParts2 at 2064 all raise ERROR.
- **Shatter's colour is static.** The `{{1,1,1,0.8}}` literal is one static float[4] shared by all shattered cards; nothing writes into it.
- **Joker with no rarity.** start_materialize gives an empty colours table when the center's rarity is missing or outside 1..4, matching the Lua's `{nil}`.
- **Ante in seed keys.** The 'stdset' / 'standard_edition' / 'stdseal' / 'stdsealtype' keys append the ante with `number_tostring` (%.14g), so an integer ante prints as in Lua.
- **Sound randomness order.** The `math.random` calls for the sounds are evaluated into locals in the Lua's left-to-right argument order.
- **Time precision.** Time arithmetic in explode is done in `num`; `G->TIMERS.TOTAL` is a float per game.h, so the wobble's sin/cos arguments have float precision.
- **Nested tables are wrapped.** Tables and arrays inside TAB/ARR are wrapped in `T(...)`, since the variadic arguments are Vals.

## card_7.c

Card_load (card.lua:4659) and Card_remove (card.lua:4727) are written.

Notes:
- **`VT.h` in load:** line 4689 is `self.VT.h = self.T.H` (nil in the Lua). The C sets `VT.h = T.h`.
- **Nil collapses in load:** the struct members take card.h's zero values, so a nil `rank`, `playing_card`, `sort_id` or cost becomes 0, a nil bool becomes false, and a nil string or table becomes NULL. `params`, `ability` and `base` are assigned straight from `cardTable`, so they are NULL if the save lacks them, exactly where the Lua would hold nil.
- **`config` in load:** it is replaced by a new table. `config.card` is left absent when `save_fields.card` is nil, and `Card_set_sprites` then gets `_front` NULL.
- **Shadow child in load:** `children.shadow` is `NEW(Moveable)` plus `Moveable_init(0, 0, 0, 0)`.
- **Name comparison in remove:** the `G.P_CENTERS` loop compares `v.name` with `ability.name` using `veq`, so nil == nil holds as in the Lua.
- **`used_jokers` in remove:** the key is cleared with `tsetv(ttab(G->GAME, "used_jokers"), key, NIL)`.
- **`G.I.CARD` loop in remove:** the Lua's `pairs` with `table.remove` and no break is an index loop over the live length, so the element shifted into slot k is skipped, as in Lua. Entries are compared by pointer only, so the Blind and Card_Character entries need no `is_a`.
- **`removed`:** the lower-case flag is set in `dyn`, per card.h.

## card_calc_2.c

Card_calculate_joker_b (card.lua:2874-3408) is translated in full. It returns true with *ret = NIL when one of the four branches (end_of_round, individual, repetition, other_joker) is entered but no joker returns, and false with *ret untouched otherwise.

Notes:
- **To Do List (2977):** `pairs(G.GAME.hands)` is walked with `titer`, so candidates come in the insertion order of `G.GAME.hands`. The hand picked by `pseudorandom_element` depends on that order, so `G.GAME.hands` must be built in LuaJIT's observed hash order for seed parity. Marked with a `// note:`.
- **8 Ball (3110):** `extra.func` is stored as `FN(fn_8ball, NULL)`. The scorer (state_events.lua:745) must call it with `fcall0(tget(extra, "func"))`.
- **Colours and cards in effect tables:** colours are `P(float *)` (e.g. `P(G->C.RED)`); `card` and `focus` are `O(self)`. `repetitions`, `x_mult`, `chips`, `mult`, `dollars` and `Xmult_mod` are the raw Val of `ability.extra` or its fields.
- **Baseball Card (3399):** the event's ctx is the context table itself and `other_joker` is read when the event runs, as the Lua closure does. The context table must therefore not be reused or mutated by the caller before the event fires.
- **Eaten jokers (2906, 2948, 3022):** the three identical closures share one static pair, `ev_eat` / `ev_eat_remove`. `self.states.drag.is = true` writes only the card's own States; if the sprites need the Lua's shared states table, the mirroring belongs to card.c.
- **Dollar buffer (3152, 3178, 3226, 3312):** the `(dollar_buffer or 0) + x` line and its reset event are factored into the static `dollar_buffer_add`, with the same order of effects.
- **Gift Card (2995, 3001):** the `if v.set_cost` test is always true (every entry is a Card) and is left as a comment.
- **Card_get_id:** it is called as many times, with the same short-circuiting, as in the Lua, because a Stone Card returns a new random number on each call.
- **Mr. Bones (3051):** `G.hand_text_area.blind_chips` / `game_chips` are juiced with `UIElement_juice_up(e, -1, NIL)`.

## card_character.c

All 11 functions of card_character.h are translated.

Notes:
- **Class objects:** this file defines `Card_Character_class` (super `Moveable_class`; slots move, hard_set_VT, draw, remove) and `Card_Character_card_class` (super `Card_class`, hover = `Node_hover`, name "Card"). `Card_Character_init` calls `class_init` on the card class if it is not ready, but `classes_init` must still cover `Card_Character_class` itself.
- **Virtual calls on children:** button and speech-bubble remove, every child draw, and the card's hard_set_VT go through `CALL`, so `UIBox_class`, `Particles_class` and `Card_class` must have those slots filled. `Card_juice_up(card, -1, NIL)` is called by name.
- **`G.P_CARDS.empty`:** it is nil in the Lua, so `Card_init` receives NULL for `card`.
- **`say_stuff` random calls:** the voice number and the pitch are drawn into locals before `play_sound`, to keep the Lua's left-to-right order (voice index first, then pitch).
- **`say_stuff` events:** the first is `after` 0.1 on the base queue; each blip is `after` 0.13 with `.not_blockable` and `.not_blocking` on the `tutorial` queue.
- **`draw`, highlight block:** `self.highlight` is always NULL. The `draw_children` call at :137 is omitted with a note, because a UIBox has no such method.
- **`remove`:** the `pairs` + `table.remove` loop over `G.I.CARD` is an index loop that moves on to k+1 after a removal, as Lua's `next` does.
- **`add_button`:** a nil colour is stored as NIL (key absent), otherwise as `P(colour)`; `yoff` NAN gives 0.3.

## card_calc_1.c

card.lua:2291-2873 is translated in full.

The two public functions are Card_calculate_joker and Card_calculate_joker_a, with the signatures in card.h. Everything else in the file is static: one jk_<context> function per large branch, the event closures, and three juice_card_until predicates.

Notes:
- **Link dependencies:** Card_calculate_joker calls Card_calculate_joker_b and Card_calculate_joker_c, which are in card_calc_2.c and card_calc_3.c.
- **cards_destroyed (2622-2670):** translated, although it looks like dead code, because `docs/conventions.md` says not to skip branches.
- **Merged closures:** identical Lua closures share one C function.
  - ev_create_consumeable serves Hallucination, Cartomancer's inner event and Sixth Sense; the type and key_append are in its ctx.
  - ev_glass_outer / ev_glass_inner serve Glass Joker at 2655 and 2693.
  - ev_xmult_text serves Glass Joker (The Hanged Man) at 2717 and Constellation at 2730.
- **Captured context:** closures that read context.blueprint_card when the event runs (Certificate, Madness, Burglar, Cartomancer, Faceless Joker) keep the context Table pointer. This relies on tables never being freed.
- **Assumed context shapes:**
  - context.blind (setting_blind) is the blind prototype table, so .boss is read with tbool.
  - context.consumeable, other_card and blueprint_card are O(Card).
  - full_hand, removed and glass_shattered are arrays of O(Card).
  - context.cards may be {B(true)}.
- **Blueprint / Brainstorm:** if the copied joker returned a truthy non-table, the C raises ERROR where the Lua would fail indexing a boolean. No joker returns one.
- **Direct calls, not CALL():** Card_juice_up and Card_remove are called by name. If a Card subclass overrides juice_up or remove, these sites would bypass it.
- **Marble Joker:** G.playing_card is incremented as an int with 0 as nil, as game.h describes.
- **Mail-In Rebate:** the id comparison is false if mail_card.id were nil while Card_get_id returns 0; the Lua's nil == nil would be true. mail_card.id is always set in a run.
- **Sound pitch:** the cosmetic math.random() calls at 2365, 2366 and 2575 use love.h's math_random().

## cardarea_2.c

CardArea_load and CardArea_remove written. Three decisions: (1) cardarea.lua:661 sets self.children = nil, but node.h says Node.children is never NULL, so CardArea_remove replaces it with a new empty table. remove_all leaves named children (area_uibox, view_deck, peek_deck) in the old table, so swapping it out stops Node_remove from removing them a second time. (2) The G.I.CARDAREA scan at :662 is an index loop that keeps going after tremove, which mirrors pairs() plus table.remove on an array (the entry shifted into k is skipped). (3) The accidental global `loading` (:642/:644) is not translated, as cardarea.h says; a grep of the Lua confirms nothing reads it. CardArea_class is not defined in this file (it belongs to the file containing cardarea.lua:1).

## card_calc_3.c

Card_calculate_joker_c (card.lua:3409-4063) is fully translated.

Notes:
- Context encoding assumed: `cardarea` is O(CardArea *), `scoring_hand` / `full_hand` are arrays of O(Card *), `poker_hands` is a table of tables, `scoring_name` is a string, `blueprint` is read by truthiness. `G.GAME.blind` is read as O(Blind *) for `->triggered` (Matador).
- `next(context.poker_hands[x])` goes through a static helper `PH()`, which raises ERROR when the hand table is missing or the name is nil, as Lua's `next(nil)` would.
- The repeated `{message = localize{type='variable',...}, <mod> = v}` returns go through the static helpers `ret_mod` / `ret_mod_col`. Key order inside the table is message, mod, colour; where the Lua writes colour before the mod (3653) only the insertion order differs.
- Midas Mask and Vampire keep a count where the Lua builds the `faces` / `enhanced` lists, since only their length is used.
- Ice Cream and Seltzer share one event closure pair (`ev_eaten` / `ev_eaten_remove`); the Lua bodies are identical. The Lua's `self = nil` there only clears an upvalue, so it is a comment.
- Vagabond, Superposition and Seance share one `create_card` event with a context holding the type and key_append ('vag', 'sup', 'sea').
- Card methods are called by name (`Card_juice_up`, `Card_remove`, `Card_set_ability`, `Card_start_materialize`), not through CALL.
- `v.vampired` is in the card's dyn, set to true and then back to NIL in the event. DNA's `_card.states.visible = nil` becomes `false`.
- DNA: `G->playing_card + 1` covers the `(x and x+1) or 1` form, since 0 stands for nil.
- `stone_tally`, `steel_tally` and `driver_tally` read as 0 when absent.
- Loyalty Card uses `lmod` (floored modulo); Bootstraps uses `floor()` on doubles.

## card_6.c

card.lua:4064-4658 translated: the 17 header functions from Card_is_suit to Card_save, plus 4 static helpers (smeared_match, ds, draw_sticker, true_or_nil).

Notes:
1. **send_to_shader lives in dyn, not a local.** card.h says self.ARGS.send_to_shader is a local of Card_draw, but the Lua writes it only in the 'shadow'/'both' pass and the 'card' pass reuses that pair (nil if no shadow pass ever ran). It is kept as a Table {a, b} under the card's dyn key "send_to_shader", which also avoids a table allocation per frame. Sprite_draw_shader gets NULL where the Lua would pass nil.
2. **ambient_tilt branch is unconditional.** card.h says ambient_tilt is never nil, so the Lua's `elseif self.ambient_tilt` became a plain else.
3. **layered_parallax is copied.** Card_set_card_area copies the area's Vec2 where the Lua shares the table by reference.
4. **Card_align calls UIDEF_card_focus_ui_set_alignment(self)** for focused_ui:set_alignment(), as UI_definitions.h prescribes.
5. **Card_save booleans.** no_ui, added_to_deck, pinned and bypass_* are saved as true or absent, because the bool members merge nil and false. debuff is always saved as a bool. rank and playing_card are absent when 0. 'highlighted' reads the dyn key 'highligted' (the Lua's typo, always nil). The literal relies on TAB/tab_of accepting NIL values as "key absent", including S(NULL) and T(NULL).
6. **Virtual calls.** Card_release calls CALL(self->area, release, dragged), since CardArea has no release of its own. Children of unknown class are drawn and removed through CALL(obj, draw) / CALL(obj, remove), and the sprites' hard_set_T goes through CALL.
7. **Card_highlight does not remove an existing use_button** before overwriting it, exactly as the Lua.
8. **Card_update, Card_draw and Card_highlight dereference G->jokers without a NULL check**, like the Lua, which errors if G.jokers is nil in a run.
9. **Card_juice_up consumes exactly one math_random() in either path**; Card_hover therefore consumes two (juice_up, then the sound pitch).
10. **Sprites' states mirroring** (the note in card_1.c's share_states) is not touched in this file.

## animatedsprite.c

- **Class object:** `AnimatedSprite_class` is defined in this file (Lua line 2), with super `&Sprite_class` and the entries `animate`, `remove`, `draw_self`, `set_sprite_pos`, `reset` and `get_pos_pixel`. The rest is inherited through `class_init`. `Sprite_init` must call `set_sprite_pos` through `CALL` for the override to run, as the Lua does.
- **Fields kept in dyn:** `self.offset_seconds` is the number `"offset_seconds"` and `self.float` is the boolean `"float"`, because node.h has no members for them. Callers that set `.float` (tag.lua:507, UI_definitions.lua:3995 and 5829) must use `tsetb(sprite->...object.dyn, "float", true)`.
- **Exact-class test:** `getmetatable(self) == AnimatedSprite` is `cls == &AnimatedSprite_class`. A sprite whose class is swapped to `UIDEF_blind_sprite_class` after init is still added to `G.I.SPRITE`, as in the Lua.
- **`AnimatedSprite_remove` keeps the Lua bug:** the undefined `k` makes `table.remove` pop the last entry of `G.ANIMATIONS` and of `G.I.SPRITE` when self is listed, and `Sprite_remove` then removes self if it is still there. This was flagged for a conscious decision; I reproduced it literally.
- **Timer precision:** `offset_seconds` is taken from the float `G->TIMERS.REAL`, and the frame index is computed in double from it. Double timers would be more exact, but game.h fixes them as float.
- **Quad allocation:** `set_sprite_pos` makes a new Quad on every call, as the Lua does, and the old one is not freed (no collector yet).
- **Ignored arguments:** `draw_self` ignores `overlay` and `animate` ignores `dt`, since the Lua takes neither.

## controller_3.c

Controller_update_focus (:1112), Controller_capture_focused_input (:1262) and Controller_navigate_focus (:1375) are translated.

Notes:
- **Scratch tables:** G.ARGS.focus_list, focusables, focus_cursor_pos, focus_vec and the accidental global _ft are file statics/locals of controller_3.c (only update_focus uses them), not entries of G->ARGS.
- **Focus sort:** uses tsort (Lua 5.1's algorithm, per `docs/conventions.md`), not a stable insertion sort.
- **Focus geometry:** focus_vec and focus_cursor_pos are float, compared against 0.1f; the Lua uses doubles, so borderline eligibility could differ.
- **.area / .rank:** read only through Card after is_a(focused, &Card_class), since only card.lua sets .area; any other node counts as area == nil.
- **capture_focused_input return:** ret is tri-state inside (the nil of :1327/:1336), and the function returns true only for exactly true, as the header says.
- **Slider hold:** held_button_times[button] is read with tnum, so a nil reads as 0 and does not slide, where the Lua would raise a comparison error.
- **Places that ERROR where the Lua would also fail:**
  - focus_args.funnel_to / funnel_from still B(true) (not yet linked by set_values).
  - focus_list[1].node nil (G.hand.cards[nu_rank] missing at :1175).
  - G.OVERLAY_MENU == OVERLAY_MENU_TRUE on a press with no screen keyboard, raised for any button; the Lua fails the same way at :1298.
- **Unguarded nils:** children[1] / children[3] of a cycle or tab node, and the neighbour card at :1283/:1289, are dereferenced without a check; the Lua would also error there.
- **Calls into other files:** Controller_is_node_focusable, Controller_L_cursor_release, Controller_update_cursor, Controller_snap_to (other controller parts); CardArea_can_highlight, CardArea_align_cards, UIBox_get_UIE_by_ID, UIBox_get_group, FUNCS_slider_descreet, EMPTY.

## event.c

Written: Event_init, Event_new, Event_handle, EventManager_init, EventManager_add_event, EventManager_clear_queue, EventManager_update, plus the definitions of Event_class and EventManager_class (both .super = &Object_class, no virtual methods).

Notes:
- Unknown timer name: any read of G.TIMERS[name] with a name TIMERS_get does not know is an ERROR(), including the read in Event_init. The Lua only crashes later, when it does arithmetic with the nil. The game uses only REAL, TOTAL and UPTIME.
- 'condition' with no config.func: the default comparison closure captures self, so self->ctx is set to the event itself and config.ctx is dropped. No caller in the game uses 'condition'.
- 'ease' or default 'condition' with none of the four ref forms set: ERROR(), the Lua's crash on a nil ref_table. For 'ease' this happens in Event_init.
- Ease on a table key whose value is nil: start_val becomes 0 through vnum, where the Lua would keep nil and crash in the arithmetic.
- created_on_pause: 1 with pause_force, otherwise from tget(G->SETTINGS, "paused") as -1 for nil, 0 for false, 1 for true. Only 0 is skipped on pause.
- Unknown queue name in add_event or clear_queue: ERROR(), as the Lua crashes on a nil queue.
- Queue order: EventManager_update and clear_queue walk the queues with titer, which relies on TAB keeping insertion order (unlock, base, tutorial, achievement, other).
- G.ARGS.event_manager_update is a local EventResults, as event.h says. v[i] is re-read by index for the handle and for the removal, and tlen is re-read on every iteration.
- Timers are floats in game.h; all event arithmetic is done in num.

## controller_2.c

All 18 Controller methods of controller.lua:575-1105 (update_axis to is_node_focusable) are translated.

Notes:
- Functions from other parts that this file calls: Controller_handle_axis_buttons, Controller_capture_focused_input, Controller_navigate_focus (all declared in controller.h).
- Debug code not translated, each left as a one-line comment: the `not _RELEASE_MODE` key block at :806-890 (including event closure :812) and the G.DEBUG toggle at :925-927. The debug_tools removal at :928-931 is not gated in the Lua, so it is translated.
- `G.FUNCS:options()` (:794) is `FUNCS_options(NULL)`: the Lua passes G.FUNCS as `e`, which the function does not use.
- repress_timer is a float member with 0 as nil, so the auto-repeat thresholds are 0.3f / 0.1f compared against a double hold time, not the Lua's doubles.
- Axis maths is in float (the Joystick API, G.CURSOR.T and G.DEADZONE are float); comparisons against 0.1 / 0.5 / 0.3 use double constants as the Lua does.
- Two static helpers read fields that are members on one class and dyn elsewhere: `node_area` (Card.area, else dyn "area") and `node_UIBox` (UIElement.UIBox, else dyn "UIBox"). `card.jimbo` is read from the card's dyn, as card_character.h says.
- G.run_setup_seed and G.setup_seed are written to G->object.dyn (game.h); G.forced_stake = nil is 0.
- `(not not node.created_on_pause) == (not not G.SETTINGS.paused)` is `node->created_on_pause == tbool(G->SETTINGS, "paused")`.
- EMPTY(list) is tclear on collision_list and nodes_at_cursor, so Controller_init must have created both tables.
- :1052 `press_node:can_drag()` goes through CALL(press_node, can_drag), so every Node subclass needs that Class slot filled by class_init.

## object.c

object.c defines Object_class (name "Object", super NULL), class_init and classes_init. The four Lua methods get no C function of their own: Object:init is empty and has no prototype in node.h; Object:extend is replaced by the static Class objects plus class_init; Object:is and Object:__call are is_a and NEW/object_new, which core.h declares as implemented in src/core.c. They are not defined here, to avoid duplicate symbols.

Notes:
- If src/core.c also defines Object_class, drop one of the two definitions.
- class_init sets ready before recursing into the super, then copies each of the 22 method slots of struct Class that the class left NULL. A slot added to struct Class later must also get an INHERIT line in class_init.
- classes_init covers all 22 classes declared with "extern Class" in include/*.h, including the three extra ones (Tag_sprite_class, Card_Character_card_class, UIDEF_blind_sprite_class). This settles the class registration point left open in `docs/header-notes.md`. Each of these must be defined, with its super set statically, by its own file, or the link fails.
- A Class added to a header later must be added to the list in classes_init.

Depends on:
- void classes_init(void); in include/needs/object.h -- node.h:28 mentions it but no header declares it; main.c includes needs/object.h and calls it once at start, before any object is made

## controller_1.c

- **Controller_class is defined here** (controller.lua:2, super Object_class, no virtual methods). The files of controller.lua 574+ must not define it again.
- **Extra include:** the file includes <ctype.h> besides all.h, for the hand-coded pattern checks.
- **Fields on other classes:** `node.area`, `node.rank` and `node.click_timeout` are read from the Card members when `is_a(node, &Card_class)`, otherwise from the node's dyn (static helpers node_area, node_rank, node_click_timeout). A Card with rank 0 stores nil in cardarea_context.
- **Exact-class test at :282:** `getmetatable(prev_target) == Card` is `cls == &Card_class || cls == &Card_Character_card_class`, because Jimbo's card keeps the Card metatable in the Lua.
- **Console matching (:116):** the eight Lua patterns, including the %f frontiers, are hand-coded as static matchers; there is no G.ARGS.gamepad_patterns table. Playstation is tested before Nintendo, where the Lua's order is hash order.
- **set_gamepad (:100):** `mapping:match("^%x*,(.-),")` is hand-coded. Sprites of the gamepad_ui atlas get `sprite_pos.y` written, then `CALL(v, set_sprite_pos, TAB("x", ..., "y", ...))` with a fresh table.
- **snap_to:** copies T into the struct, where the Lua keeps the caller's table by reference.
- **process_registry:** the `registry[i].node.click` existence test is always true in C. A NULL node or entry is skipped, where the Lua would error.
- **update_cursor:** if put_focused_cursor does not write x/y (the Lua's arithmetic error case), cursor_position keeps its old value.
- **pairs order:** the pressed/held/released maps, locks and button_registry are walked with titer (insertion order). handle_axis_buttons walks l_stick, r_stick, l_trig, r_trig.
- **Events:** E1 (:199) and E2 (:400) are the statics ev_unlock_frame and ev_touch_hover, using the inverted not_blocking / not_blockable flags.
- **Nil-valued table entries:** TAB(...) is called with possibly-NIL values (`O(NULL)` for node, a nil `cursor_pos`). This assumes tab_of treats a NIL value as "key absent".

## cardarea_1.c

The 21 CardArea methods of cardarea.lua:5-630 are translated (init through save).

Notes:
- `Class CardArea_class` is defined here (Lua line 2), with static wrappers for update, move, draw, remove, click and hard_set_T. `CardArea_remove` itself is in cardarea_2.c. Nothing here calls `class_init` on it; that is left to `classes_init`.
- The per-frame sorts in `align_cards` and the sorts in `CardArea_sort` use `tsort` (Lua 5.1's algorithm), as `docs/conventions.md` and cardarea.h say. A stable insertion sort would not give the same order on ties.
- Layout math in `align_cards` is float (`sinf`, `G->TIMERS.REAL` as float). The one exception is the joker comparator (:528), which compares in double so `100*sort_id` does not swamp the position.
- The repeated row-x expression (:441, :456, :471, :486, :500, :514) is one static helper, `row_x(self, card, k, W)`. The repeated one-text rows of the UI definitions are a static `text_row()`. Both build the same values the Lua writes inline.
- UI definitions are Tables with `n = N(UIT_x)`, colours as `P(float *)`, and `ref_table = T(self.config)` for card_count / card_limit. The area row has `ref_table = O(self)`, and `major` / `parent` are `O(self)`.
- The `{0,0,0,0.1}` background colour (:294) is a calloc'd float[4] per area box, never freed.
- The conditional entries in `nodes` (:295 and :308) are built without holes: the entry is simply absent when the Lua's is nil.
- `G.view_deck[1].cards` (:411) accepts either an `O(CardArea)` or a table with a `cards` key. `G.view_deck` is never assigned by the game, so this early return is dead in practice.
- `CardArea_remove_from_highlighted` with a NULL card that passes the early return raises `ERROR`, as the header says.
- `CardArea_draw_card_from` ignores its `stay_flipped` argument, because the Lua shadows it. The RNG order is the blind's `stay_flipped` first, then `pseudorandom(pseudoseed('flipped_card'))`.
- `change_size` queues one default Event with a `{self, delta}` context. That event calls `draw_card(G.deck, G.hand, i*100/card_count, NULL, false, NULL, 0.07, false, false, -1, false)` and then queues a default sort event, once per card.
- `draw_layers` is computed once on the first draw and kept in the struct member, per cardarea.h.

## save_manager.c

All six io.h prototypes are implemented. Two static helpers: prefix_profile_of (the "(profile or 1)..''" + create directory + '/' sequence at :20-22, :64-66, :77-79) and meta_sub ("meta.x = meta.x or {}" at :29-31).

Notes:
- Lines 1-12 (requires, jit.off, CHANNEL) and the CHANNEL:push('done') at :54 have no C counterpart, since there is no thread or channel; each is left as a comment.
- Table saves go through compress_and_save_table rather than compress_and_save(file, STR_PACK(t)), including meta.jkr at :49. io.h defines the two as equivalent, minus the text that would live forever.
- string.find is translated as plain str_find. For 'u'/'d'/'a' this is identical. For save_notify it matches the Lua only while unlock keys contain no Lua pattern characters.
- UDA keys are read and written with tgetv/tsetv using the iterator's key Val, so a non-string key would behave as in the Lua.
- save_manager_request with type 'save_settings' and a missing profile_num writes settings.jkr first and then raises ERROR, matching the Lua, where :57 runs before the concatenation at :58 fails.
- A non-table meta.unlocked/discovered/alerted read from meta.jkr raises ERROR, as the Lua's index would.
- Return values of love_filesystem_createDirectory and love_filesystem_append are ignored, as in the Lua.

## node.c

All 19 Node_* functions of node.h are translated.

Notes:
- node.c defines `Class Node_class` (node.lua:2), super `&Object_class`, which comes from object.c. It fills update, draw, remove, hover, stop_hover, click, release, drag, stop_drag, can_drag, animate, collides_with_point, set_container and put_focused_cursor through static wrappers. The other slots (move, hard_set_T, hard_set_VT, juice_up, the sprite ones) stay NULL because Node has no such methods.
- The `drag` slot wrapper drops the offset, since Node:drag() takes none.
- Node_init only creates `config` and `children` when they are NULL, as the Lua's `self.config or {}`. It writes the default `T = {}` into the caller's args table, as the Lua does.
- `created_on_pause` is a bool member, so the nil of G.SETTINGS.paused at boot becomes false.
- "getmetatable(self) == Node" is `self->object.cls == &Node_class`. A private class with Node as super would not be added to G.I.NODE, same as the Lua.
- Node_draw_boundingrect keeps only the under_overlay assignment; the G.DEBUG drawing (node.lua:94-123) is left as a comment, per `docs/conventions.md`.
- Node_drag keeps the Lua's slip at line 240 (it indexes children.h_popup, not d_popup) and raises ERROR() when there is no h_popup, as Lua would. Node_hover raises ERROR() when config.h_popup_config is nil (line 270).
- Children are walked with titer (array part, then named keys in insertion order) and called through CALL(child, draw / set_container / remove). Every value in `children` must therefore be an O(object) whose class has those slots filled.
- Node_stop_drag removes from G.I.POPUP with no break, as the Lua; Node_remove removes the first match from each list, on the current G.STAGE's list.
- Node_collides_with_point returns false where the Lua returns nil.
- Node_fast_mid_dist is the literal expression (sqrt over the x term only, NaN for a negative dx); nothing in the game calls it.
- Geometry is float (fabsf, cosf, sinf, the 0.1f thresholds), per `docs/conventions.md`.

## particles.c

- Class object: particles.c defines `Class Particles_class` (particles.lua:2) with update, move, draw and remove; the rest is inherited from Moveable through class_init. The draw slot is a static wrapper calling `Particles_draw(self, -1)`.
- Config encoding assumed by Particles_init: `attach` is `O(node)` and is passed straight to `Moveable_set_alignment` as `major`; `colours` is an array of `P(float *)`; the default is `ARR(P(G->C.BACKGROUND.D))`, by pointer so it follows the animated colour. A NULL config is treated as `{}`.
- Unknown timer_type: `*TIMERS_get(self->timer_type)` is dereferenced unchecked, so a `config.timer_type` that is not a G.TIMERS key crashes on NULL where the Lua would error on nil arithmetic.
- Particle storage: `self->particles` is a realloc-grown array (starts at 16, doubles), with no fixed cap, so behaviour matches the Lua's unbounded table. `Particles_remove` frees it and zeroes `nparticles`/`room` before `Moveable_remove`; this free is a C addition with no Lua counterpart.
- Fade: `Particles_fade` uses `.ref_float = &self->fade_alpha`. The pointer dangles if the Particles object is freed while the ease is still queued (noted at the call, as event.h asks).
- RNG order: spawn draws are written one statement per field, in the Lua's order: offset x and y (only if fill), dir, facing, size, velocity, r_vel, then `pseudorandom_element(colours, NIL, NULL)`.
- Pre-warm: the `initialize` loop calls update and move through `CALL` (virtual dispatch, as `self:update` / `self:move`).
- Remove from the major's children: only the array part of `role.major.children` is walked, continuing after a removal with no break, as the Lua's pairs + table.remove does.
- Number types: particle maths is float (the geometry convention); `max` stays num (default 1e15); `pulse_max` is truncated to int.

## moveable.c

All 21 Moveable methods of node.h are translated (Moveable:init is the two constructors Moveable_init and Moveable_init_args).

Notes:

- **Definitions this file owns:** it defines `Moveable_class` (super `Node_class`; slots move, draw, remove, drag, hard_set_T, hard_set_VT, juice_up; the rest inherited through `class_init`). It also defines `role_type_of` and `bond_of`, which node.h declares in the Moveable section and nothing else defined. node.c must not define them too. An unknown string reads as 'Major' / 'Strong'.
- **MWM (moveable.lua:343):** the global scratch table does not exist in C. It is locals of `Moveable_move_with_major`, since every field is written before it is read and nothing else uses it.
- **Offsets are copied, not shared:** `Moveable_set_alignment` and `Moveable_set_role` copy `args.offset` into the `Vec2` members, where the Lua keeps the caller's table by reference. A caller that later mutates its own offset table (e.g. a UIBox `config.offset`) must also write `alignment.offset` / `role.offset`. Both functions still set `args.offset = nil` on the caller's table when it is malformed, as the Lua does.
- **Glue aliasing:** `Moveable_glue_to_major` shares T by pointer (`self->node.T = major->node.T`), but `pinch` and `shadow_parrallax` are by-value members, so they are copied at every glue (once a frame from `Moveable_move`). A change to the major's is seen at the glued node's next move.
- **Bond and role strings:** `set_role` / `set_alignment` args carry them as strings (`S("Minor")`, `S("Weak")`), matching existing callers in card_1.c and card_character.c.
- **`Moveable_init_args(NULL)`** behaves as the Lua's `Moveable()` with no argument: T = {0, 0, 0, 0}, not Node's w = h = 1 defaults.
- **`Moveable_hard_set_T`** assigns its floats as given. It does not handle the NAN-as-nil convention of card.h / cardarea.h; those classes must resolve nil before calling it.
- **Frame cache:** `FRAME.MAJOR` / `OLD_MAJOR` map to `frame_major.have` / `.tab`. `G->REFRESH_FRAME_MAJOR_CACHE > 0` forces a recompute.
- **`self.juice = nil`** zeroes the whole juice struct (`on = false`). `Moveable_juice_up` resets `handled_elsewhere` to false, as a new table would.
- **Virtual calls:** `Moveable_drag` calls each child through `CALL(child, drag, offset)`, iterating `children` in titer order (array part, then named keys in insertion order). `Moveable_move` calls the major's move through `CALL(major, move, dt)`. The other self-calls are direct, because no Lua subclass overrides those methods.
- **Unseeded random:** `Moveable_juice_up` calls `pseudorandom_element` with a NIL seed on {0.6*amount, -0.6*amount}, only when `rot_amt` is nil or false.
- **`Moveable_get_major` on a non-Major with no major** dereferences NULL, where the Lua raises an index-nil error; there is no guard.

## sound_manager.c

All 9 io.h prototypes are implemented, plus the definitions of SOURCES and DISABLE_SFX.

Notes:
- Music and ambience depend on the preload. sound_manager_init keeps only directory entries whose last four characters are ".ogg", as the Lua does. MODULATE and AMBIENT only start sounds for keys already in SOURCES, so if the PS2 pack lists only "x.ogg.snd" under resources/sounds, music and ambience never start.
- Iteration order: pairs(SOURCES) is walked in insertion order, i.e. the sorted directory order. This only changes which music key trips the restart check.
- Replacing a key mid-walk: RESTART_MUSIC replaces SOURCES[k] while iterating SOURCES. This relies on tset of an existing key keeping its slot during a TIter walk.
- Nil handling: where the Lua would raise on a nil, the C raises ERROR instead of reading 0. This covers args.dt, pitch_mod, splash_vol, sound_settings and its fields, ambient_control, s.sound and a nil sound_code. Two static helpers (arith, snd) do this.
- RESTART_MUSIC(NULL): it stops and clears the first music key, then raises ERROR, as the Lua does. With no music keys it does nothing. The "restart_music" request calls it this way.
- "reset_states" request: translated inline as in the Lua (lines 200-204), copying request.state as a Val, so a nil state clears created_on_state. RESET_STATES(int) is separate and, per io.h, must not be reached from game.lua:1524.
- Platform types: volume and pitch are computed in double, left to right as written, and cast to float only at Source_setVolume / Source_setPitch.
- Not translated: the LOAD_CHANNEL pushes, CHANNEL and jit.off have no equivalent and are left as comments.

## string_packer.c

All 7 io.h packer prototypes are implemented (STR_PACK, STR_UNPACK, get_compressed, compress_and_save, compress_and_save_table, table_pack, table_unpack).

Notes:
- STR_PACK raises ERROR on a V_PTR or V_FN value (the Lua fails there too, concatenating userdata or a function). Any table that gets saved (G.GAME, G.SETTINGS, a profile, the save_run table) must not hold a colour as P(float *) or any other raw pointer, or the save aborts.
- STR_UNPACK is not loadstring. It reads only what STR_PACK writes, with free whitespace: `return {[key]=value,...}`, where key and value are a quoted string (Lua escapes), a number, true, false, nil or a nested table. Positional fields and `name=value` fields fail to parse. STR_UNPACK then raises ERROR; table_unpack returns NULL.
- A bare name such as `inf` or `nan` in a PC save reads as nil, so the field is absent, as in the Lua. `-inf` and `-nan` are a parse failure, which matches the Lua's arithmetic-on-nil error.
- Tables built before a parse error are left allocated.
- %q is written in the Lua 5.1 form: backslash before `"`, `\` and newline, and CR as `\r`. LuaJIT on the PC writes other control characters as `\ddd`; the reader accepts both, but output is not byte-identical to a PC save for strings holding such characters. A `\0` escape truncates the C string.
- Entry order is the table's insertion order (titer); the Lua's pairs order is arbitrary and nothing depends on it.
- get_compressed returns an interned string that lives forever, as io.h requires. Code that loads big saves often should use love_filesystem_read plus love_data_decompress plus table_unpack instead.
- get_compressed raises ERROR if getInfo succeeds but the read returns NULL (the Lua would fail at string.sub(nil)).
- compress_and_save ignores the result of love_filesystem_write, as the Lua does, and raises ERROR if love_data_compress returns NULL.

## sprite.c

All 11 functions of sprite.lua are translated, and Sprite_class is defined here (super Moveable_class; draw, remove, draw_self, set_sprite_pos, reset, get_pos_pixel).

Notes:

1. **sprite_pos by reference.** node.h has sprite_pos as a Vec2, but the Lua keeps the caller's table and re-quads when it changes. The table is kept in the sprite's dyn under "sprite_pos" and the Vec2 member mirrors it. Sprite_draw_self compares the dyn table against sprite_pos_copy first, then the member, so either way of mutating it triggers the re-quad.

2. **Quad reuse.** Sprite_set_sprite_pos allocates the Quad once and afterwards calls Quad_setViewport, instead of a new quad per call as the Lua does (there is no collector). Anyone holding a sprite's Quad pointer sees it change.

3. **Draw major fields.** Sprite_draw_shader reads tilt_var, hover_tilt, dissolve and dissolve_colours from struct members when the draw major is a Card or a Blind. For anything else (tag sprite, G.SPLASH_LOGO) it reads dyn: "tilt_var" as a table {mx, my}, "hover_tilt" and "dissolve" as numbers, "dissolve_colours" as an array of P(float *).

4. **Custom send entries.** Read order is val, func (if it returns truthy), ref_float, ref_colour, ref_table[ref_value]. A value may be a number, a bool, P(colour) or an array table of 2 to 4 numbers. For button_callbacks.lua:2010-2011 there is an encoding the headers do not define: ref_table = O(sprite) with ref_value "image_dims" or "get_pos_pixel" sends that sprite's dims / pos pixel; any other object ref_table reads its dyn. button_callbacks_*.c must match it.

5. **Non-custom _send** (the card edition {a, b} pair) is sent as a vector of tlen(_send) floats, capped at 4.

6. **G.I.SPRITE registration.** Sprite_init registers only if cls == &Sprite_class at init time. Sprites with private subclasses (Tag_sprite_class, the stake-8 sprite, UIDEF_blind_sprite_class) are plain Sprites in the Lua and must swap class after init to stay listed.

7. **Sprite_draw_from** uses other_obj->scale_mag only; the Lua's `or other_obj.VT.scale` fallback is unreachable because the header types other_obj as Sprite.

8. **Sprite_remove** stops at the first match in G.ANIMATIONS and G.I.SPRITE (entries are unique, so this equals the Lua loop without break).

9. **Dyn fields read:** "shader_tab" and "video" on the sprite.

Not ported:
- sprite.lua:134 self.video draw path in Sprite_draw_self (love.h has no videos); aborts only if dyn 'video' is set (only UI_definitions.lua:2853, the demo video CTA, sets it)
- sprite.lua:202 self.video:release() in Sprite_remove; same condition

## text.c

All 9 DynaText functions of ui.h are translated (init, update, update_text, pop_out, pop_in, align_letters, set_quiver, pulse, draw), plus one static helper (cfg_or).

Notes:
- text.c defines `Class DynaText_class` (text.lua:2; super Moveable, entries update and draw). ui.c must not define it again.
- `DynaText_init` sets `node.config` before calling `Moveable_init`, relying on `Node_init` keeping an existing config (node.c:57). T stays NULL during the two `update_text` calls of init, as ui.h says.
- `ui_object_updated` and `non_recalc` are written to the object's dyn. `non_recalc` is copied raw from the config (nil stays nil), not from the bool cache.
- config.W / config.H are stored in the config table as float-valued numbers (they pass through `DynaString.W/H`), so the `T.w ~= config.W` change test is float against float and does not fire every frame.
- A part's value is read with `ui_ref_get(ref_table, ref_value)`, so it depends on ui.c implementing that.
- The letters array is allocated with one slot per byte of the string (at least the character count). Old letters' `Text` are released when a string is rebuilt.
- DynaText has no `remove` in the Lua, so nothing frees `strings`, `letters` or their `Text` when an object is removed; the Lua left that to its collector. This is a leak on the PS2 unless someone adds a release.
- `self.string` is NULL until the first `align_letters`; the Lua's `{'HELLO WORLD'}` fallback is never read.
- `config.spacing` of 0 is treated as nil in draw (no translate), which gives the same picture.
- `_shadow_norm` is left literal: it is NaN if `shadow_parrallax` is (0,0), as in the Lua. It is not guarded; Moveable keeps y = -1.5, so it should not occur.
- Lua runtime errors are kept as `ERROR()`: nil `config.string` (:73), a table part with neither `ref_table` nor `string` (:111), and a `config.string` that grew after init (:134).
- Random calls match the Lua: `math_random_range(1, #strings)` only when `random_element` is set, and `math_random()` only when the pop-in sound actually plays (object inside the room).

## ui_2.c

ui.lua:631-1054 is translated into src/ui_2.c.

The 12 functions are UIElement_update_object, draw_self, draw_pixellated_rect, update, collides_with_point, click, put_focused_cursor, remove, hover, stop_hover, release, and is_UI_containter. There is one static helper, draw_focus_outline, which holds the focus-outline block that the Lua repeats at :781-790 and :820-829.

Notes:

1. **FUNC_TRACKER not translated.** G.ARGS.FUNC_TRACKER (ui.lua:939, :949) is a counter that nothing in the game reads. It is left as a one-line comment.
2. **Cached config keys.** Where ui.h declares a `self->cfg.*` copy, the code reads the copy, not the config table: force_collision, button_dist, vert, shadow_colour, r, emboss, focus_with_object, line_emboss, ext_up, res, progress_bar, role, ref_value. This depends on UIElement_init (ui_1.c) filling them.
3. **Missing G.FUNCS name.** If `config.func` or `config.button` names a function that is not registered, the code calls ERROR(), where the Lua would fail calling nil.
4. **Non-virtual calls.** Moveable_set_role and Moveable_move_with_major are called by name on config.object, because Class has no entry for them and no Lua class overrides them. `object:draw()`, `object:remove()` and `parent:release()` go through CALL.
5. **pixellated_rect.** It is allocated with calloc on first draw and freed in UIElement_remove, as ui.h asks; the Lua has no equivalent line. The Lua's "vertex list is empty" test becomes "an emboss type was asked for and the cache was built without _emboss", the only case where it can be true.
6. **click after the callback.** Like the Lua, click keeps using `self` after the button callback, which may have removed the element. This is safe only while Node_remove / Moveable_remove do not free the object, which is the case in node.c and moveable.c today.
7. **put_focused_cursor.** The 'tab' case walks the children with titer and dereferences `children[1]` of each, as the Lua does. With no chosen tab, *x and *y are left untouched.
8. **Table shapes.** `h_popup` is stored as T(definition table) and `h_popup_config` as {align, offset = {x, y}, parent = O(self)}. G.ROOM.jiggle and G.OVERLAY_TUTORIAL.button_listen are read from the objects' dyn tables, as game.h says.
9. **Precision.** Drawing math is float; time stamps are num compared against the float G->TIMERS.REAL.

## button_callbacks_1.c

All 32 header functions whose definition starts in lines 1-648 are translated (FUNCS_tut_next through FUNCS_your_collection_tarot_page).

Notes:

- **Writes through a widget binding:** toggle_button, slider, slider_descreet and test_framework_cycle_callback write `ref_table[ref_value]` through a static `ref_set`. For a table it uses `tsetv`; for an object (`ref_table = G` for `run_setup_seed`) it writes the object's dyn table. Reads go through `ui_ref_get`, so `ui_ref_get` must read G's dyn for those keys, as ui.h and game.h say. The other parts of button_callbacks may need the same helper; this one is static.
- **can_continue with an unreadable save:** if save.jkr exists but `get_compressed` returns NULL, the Lua crashes indexing nil. The C reads VERSION as nil and returns "cannot continue". The VERSION check is `strcmp` against "0.9.2".
- **`G.FILE_HANDLER.force = true`** (load_profile's second event, unlock_all) raises ERROR if `G->FILE_HANDLER` is NULL, matching the Lua's index-nil failure.
- **Blind states:** HUD_blind_visible writes the blind's own `states.visible` and then calls `Blind_sync_states`, per blind.h.
- **Slider width:** `c.config.w` is stored from `c->T->w`, so it carries float precision; the Lua keeps a double there. The bound value itself is computed and stored as a double.
- **Missing callback name:** slider and option_cycle raise ERROR when `funcs_lookup_args` returns NULL for a set `callback` / `opt_callback` (the Lua would fail calling nil).
- **Shared code inside the file:** the two two-press confirms (delete_profile :279-286, unlock_all :327-334) share one static helper and the same two event functions; the three unlock loops (:337-357) and the collection-clear prelude (:604-610, :629-635) are also single static helpers. Events, delays, flags and order are unchanged.
- **Assumed encodings:** `e.config.ref_parent` (can_buy) is read as `O(Node)` — it is `base_attach` from UI_definitions.lua:336-343. `G.OVERLAY_TUTORIAL.Jimbo`, `step` and `step_complete` are read from the UIBox's dyn, per game.h. `G->your_collection` is read as `T(array of O(CardArea))` in the page callbacks.

## button_callbacks_3.c

All 63 functions of lines 1299-1936 are written (62 header prototypes starting in range plus FUNCS_refresh_contrast_mode_fn). Registration (funcs_register_all) is left to the last part, as the header says.

Notes:
- scale_number: math.log(x, 10) is computed as log2(x) * (1/log2(10)), the form LuaJIT 2.1's lib_math.c is believed to use (not checked against the LuaJIT source when written; `docs/development-log.md` has what the comparison runs found); it decides the floor at exact powers of ten. The arithmetic is done in double and returned as float, per the header.
- FUNCS_refresh_contrast_mode: the G.I.CARD loop skips entries that are not is_a Card_class (Blind, Card_Character), assuming they have no config.card as in the Lua.
- FUNCS_blind_chip_UI_scale: only tests that G.GAME.blind exists, since Blind.chips is a num member and never nil.
- FUNCS_warn_lang: disable_button is written to e.config (the table), as the Lua does at :1779/:1784, not to the UIElement member.
- FUNCS_change_challenge_description(NULL): id is S("nil"), the e.config.chosen write is skipped and old_chosen becomes nil, per the header.
- FUNCS_start_setup_run: the seed is read as a raw Val from G's dyn so an empty setup_seed stays truthy and is passed on, as in the Lua; forced_stake 0 is treated as nil.
- FUNCS_wait_for_high_scores and FUNCS_quit_cta (HTTP / demo only) are translated but unreachable in the port; create_UIBox_high_scores_filling is a TODO_PORT in its own file.
- The simple openers go through a local OPEN_OVERLAY macro that sets paused before the definition is built, matching the Lua's order. In toggle_seeded_run, settings and quit_cta the definition parts are built into locals first to keep the Lua's evaluation order.
- A nil element where the Lua would crash (e.g. tab_contents missing in change_tab, the infotip missing in warn_lang) is a NULL dereference in C; no guards were added.
- Event flags: notify_then_setup_run uses not_blockable for both events; warn_lang uses not_blockable + not_blocking with delays 0.06 and 0.35; change_lang uses no_delete + not_blocking; quit_cta uses after 2.5 + not_blockable.

## ui_1.c

Translated: all 14 UIBox functions (ui.lua:18-340) and the 12 UIElement functions from init to update_text (ui.lua:350-617). UIElement_update_object (:631) onwards belongs to the next part.

Notes:
1. **Class objects.** `UIBox_class` and `UIElement_class` are defined here (ui.lua:2 and :348). `UIElement_class` points at update, remove, hover, stop_hover, click, release, collides_with_point and put_focused_cursor, which the second part must define. `DynaText_class` is not defined here.
2. **`ui_ref_get` is defined here as a weak symbol.** ui.h says "implemented in ui.c" without naming a part, so a strong definition in the second part wins at link time. It dispatches a table through `tgetv`, a Card through `Card_field`, a Blind through `Blind_field`, a DynaText's `focused_string` to its member, and anything else (G's `setup_seed`) to the object's dyn. A nil `ref_value` gives NIL.
3. **`text_drawable` is released in `UIBox_calculate_xywh`.** Where the Lua sets `config.text_drawable = nil` (:135), the C calls `Text_release` first, because there is no collector. Any other code that frees that `Text` (for example `UIElement_remove`) must also clear the config key, or it will be freed twice.
4. **B or O node with no size.** `calculate_xywh` returns false so the parent skips it, as the Lua does. `content_dimensions` gets 0 and `set_values` receives 1 (Node.init's default) for the missing dimension, on a recalculate too, where the Lua would write nil into T. Consequently the nil-size else branch of `set_wh` (:562-565) cannot run in C; it is noted in a comment there.
5. **Duck typing follows ui.h.** `v.draw_self`, `v.set_wh`, `v.align` and the like are `is_a(v, &UIElement_class)`, so a non-UIElement keyed child only gets `CALL(v, draw)`.
6. **`UIBox_drag`** passes dt = 0 to `Moveable_move` for the root, where the Lua passes an undefined global (nil).
7. **`UIElement_init` fills every `self->cfg.*` cache.** The layout code in this file still reads the config table directly.
8. **`set_parent_child` and Card states (:250).** It sets `object.states.click.can = false` on the object only. For a Card, mirroring that to its sprites (shared states in the Lua) is card.c's job, per the card.h notes.
9. **Definition `nodes`** are walked with `titer`; any non-table entry is skipped as a hole.
10. **Missing callbacks abort.** A `config.func` name with no registered function raises ERROR, matching the Lua's call of nil. `UIBox_init` also raises ERROR at :93 when `args.config` is nil, as the Lua does.
11. **Debug functions.** Both `print_topology` functions are translated, since the header declares them. Floats are printed with `number_tostring`, so they show float noise.

## button_callbacks_4.c

All 22 header functions whose definition starts in 1937-2403 are translated (hand_chip_total_UI_set through check_for_buy_space).

Notes:

1. **`alignment.offset.py` dyn key (cross-file agreement needed).** `Moveable.alignment.offset` is a Vec2 with no `py`, and button_callbacks.h only says it lives in the UIBox's dyn without naming the key. The dyn key string is `"alignment.offset.py"` (macro `PY_KEY`, static helpers `py_get` / `py_save` / `py_restore`). Only button_callbacks.lua uses this field, so the part that holds `FUNCS_end_consumeable` (2565+) and any other later user must use the same key, or the saved y is never restored.

2. **Restore with `py` nil.** Where the Lua does `offset.y = offset.py` unconditionally (2271, 2287-2296) and `py` is nil, it writes nil into `offset.y`. A float cannot hold that, so `py_restore` leaves y unchanged in that case (marked with a `// note:`).

3. **Flame shader send list.** The two entries whose Lua `ref_table` is the sprite / its RETS (`image_dims`, `get_pos_pixel`, lines 2010-2011) are written as `ref_table = O(sprite)` with `ref_value = "image_dims"` / `"get_pos_pixel"`. This matches the special case already present in `src/sprite.c` (`send_custom`). The other entries keep `ref_table = T(G.ARGS[arg_tab])`; `colour_1` / `colour_2` in that table are `P(float *)`.

4. **`math.log(x, 5)`** is written as `log(x)/log(5)`.

5. **`G.C.UI_CHIPLICK` / `UI_MULTLICK`**: "all four channels zero" is treated as the Lua's nil and initialised to {1,1,1,1}, as game.h specifies.

6. **`FUNCS_use_card`**:
   - `card.from_tag` is read from the card's dyn (Card has no such member); tag.c, which sets it, must write it there.
   - `e.disable_button` uses the `UIElement` member.
   - Every write to `e` is skipped when `e` is NULL, as the header requires.
   - The closure context copies `dont_dissolve` and `delay_fac` just before the 'after 0.2' event is queued, which is their final value in the Lua too.

7. **Dependencies on other parts**: `scale_number` and `FUNCS_end_consumeable` are called through their button_callbacks.h prototypes and are defined in other parts of this file.

8. **`FUNCS_skip_tutorial_section`** reads `skip_steps`, `Jimbo` and `content` from `G->OVERLAY_TUTORIAL`'s dyn (per game.h) and removes Jimbo / content through `CALL(obj, remove)`.

## button_callbacks_2.c

All 36 functions whose definition starts in lines 649-1298 are translated (your_collection_spectral_page through RUN_SETUP_check_back_stake_highlight).

Notes:
- **Text input bound to G.** A text input's `text.ref_table` can be a table or an object (the seed input binds G). Reads go through `ui_ref_get`; writes go to the table, or to the object's `dyn` when it is an object (G's `setup_seed` is in dyn per game.h). There is no `ui_ref_set` in ui.h, so this is a static helper `text_ref_set` in this file.
- **Pattern find in `text_input_key` (:994).** `string.find(lower_ext, key)` is a Lua pattern find and core.h has no matcher, so the cases are hand-written in `lower_ext_find`:
  - `.` matches at position 1 (gives `!` with caps).
  - `^`, `$` and an empty key match empty, so the key becomes `''`.
  - `%`, `(` and `)` raise `ERROR` as the Lua's malformed pattern does (`%` is also the nil-key default).
  - longer key names use a plain find, which gives "no match" for every real key name.
  This path only runs for extended-corpus inputs (the profile name).
- **Colours in the hook config.** `hook_config.colour` and `orig_colour` are treated as `P(float *)`. `copy_table` of a colour allocates 4 new floats that are never freed, one per first key press on an input and one per RETURN.
- **`FUNCS_apply_window_changes`.** Translated in full against the SETTINGS tables, but `love_window_updateMode(int w, int h)` in love.h takes only the size, so the Lua's flags table (fullscreen, fullscreentype, vsync, resizable, display, highdpi) is dropped. It raises `ERROR` if `WINDOW.DISPLAYS[selected_display]` is nil, as the Lua does, so `GET_DISPLAYINFO` must have filled DISPLAYS before the boot call with `true`.
- **`G.your_collection`.** It is a `Val`; when it holds a single CardArea, the page functions and `INIT_COLLECTION_CARD_ALERTS` see length 0, as the Lua's `#` on that object.
- **`change_collab`.** The loop over `G.I.CARD` skips non-Card entries (Blind) with `is_a(&Card_class)`, standing in for the Lua's `v.config.card` test.
- **`G.viewed_stake`.** The member is an int with 0 as nil; comparisons with `e.config.id` use `N(viewed_stake)` or `NIL`.
- **`MODIFY_TEXT_INPUT` at position 0.** A BACKSPACE at cursor 0 writes `letters[0] = ''` exactly as the Lua (the key goes to the table's hash and nothing reads it); making it a no-op would give the same result.
- **`TRANSPOSE_TEXT_INPUT`.** It raises `ERROR` if no `'position'` child exists or there is no hook, where the Lua errors on nil.
- **Dependencies on other parts.** `FUNCS_change_tab` (:1299) belongs to the next part and is called from `apply_window_changes`. `FUNCS_change_selected_back` is registered `[args]` per the header; registration itself is done by `funcs_register_all` in the last part.

## button_callbacks_6.c

Translated FUNCS_show_main_cta (:3040), FUNCS_wipe_on (:3061) and FUNCS_wipe_off (:3129), plus 5 static event closures.

Notes:
- **One deliberate deviation from the Lua:** the three wipe_off events (W1 :3132, W2 :3168, W3 :3184) return quietly when G.screenwipe is NULL, where the Lua would raise an error. Each guard has a `// note:`. Replace the guards with ERROR() if a Lua-style abort is preferred.
- **G.screenwipe.colours** is stored in the box's dyn as `{black = P(float *), white = P(float *)}`. Game_draw must read it with `UI_colour(ttab(G->screenwipe->moveable.node.object.dyn, "colours"), "black")`. The two alpha eases use `.ref_float = &colour[3]`; both colours are heap-allocated and never freed, so the pointers stay valid after the box is removed.
- **G.screenwipe.children.particles** is `O(Particles)` under the key "particles" in the box's Node.children. As in the Lua, W3 calls Particles_remove and then UIBox_remove while the key is still set, so the particles are removed twice.
- **Holey nodes array:** the inner row's `nodes` keeps the Lua's indices, so with no message the card node is at index 2 only. This relies on UIBox_set_parent_child walking `nodes` with titer, as ui.h says.
- **states.hover.can:** `G.screenwipecard.states.hover.can = false` writes only the card's own States, not the sprites that share it in the Lua. card_4.c does the same.
- **Random call:** `pseudorandom_element(G->P_CARDS, NIL, NULL)` is unseeded (global math.random), as in the Lua.
- **Message strings:** the DynaText config gets `string = S(v)` for each entry of the message array (`S("")` if falsy).
- **show_main_cta** is translated in full, although it is demo only. It calls create_UIBox_demo_video_CTA and FUNCS_overlay_menu.

## button_callbacks_5.c

All 15 G.FUNCS of lines 2404-3039 are translated (buy_from_shop through go_to_demo_cta).

Notes:
- **`alignment.offset.py` key:** it is stored in the UIBox's dyn under the key "alignment.offset.py" (a local `PY_KEY` define). `button_callbacks_4.c` uses the same string, so `use_card` and `end_consumeable` agree.
- **`hover_tag_proxy` pause comparison:** `e.created_on_pause == G.SETTINGS.paused` is compared as booleans, because `Node.created_on_pause` is a bool. The Lua's nil-versus-false difference is lost; there is a `// note:` at the spot.
- **`end_consumeable` with nil `PACK_INTERRUPT`:** `G.STATE` becomes 0 where the Lua would leave it nil, since `STATE` is an int.
- **`buy_from_shop` inflation event:** `if v.set_cost` over `G.I.CARD` is `is_a(v, &Card_class)`, since that list also holds the Blind and Card_Characters.
- **`reroll_boss` evaluation order:** `create_UIBox_blind_choice('Boss')`, `get_blind_main_colour` and `mix_colours` are evaluated into locals in the Lua's left-to-right order before the call, because the blind choice may poll 'orbital'.
- **`reroll_shop` with no card:** `create_card_for_shop` may return NULL; it is passed straight to `CardArea_emplace` and `juice_up`, where the Lua would also fail.
- **Class dispatch:** `set_role` on a UIBox or UIElement calls `Moveable_set_role` directly, because `Class` has no `set_role` slot. UIBox and Particles removals call `UIBox_remove` / `Particles_remove` by name; card children and the alert use `CALL(..., remove)`.
- **Shared helpers:** the three identical "tags new_blind_choice with break" loops share one static helper, `tags_new_blind_choice`. `ev_delete_run` is shared by `start_run`, `go_to_menu` and `go_to_demo_cta`.
- **Dependencies in other parts:** `FUNCS_wipe_on`, `FUNCS_wipe_off`, `FUNCS_use_card` and `FUNCS_check_for_buy_space` are used through `button_callbacks.h`.

## common_events_3.c

check_for_unlock, unlock_card, fetch_achievements and unlock_achievement are translated (common_events.lua:1163-1771).

Notes:
- Steam branches (lua 1693-1702 and 1742-1757) are one-line comments; `not G.STEAM` is taken as always true.
- fetch_achievements builds G.ACHIEVEMENTS from a static {name, tier} array in source order, with order = position and steamid = fmt("BAL_%02d", order), instead of 31 inline TAB literals. Contents are the same as the Lua table.
- unlock_achievement passes the interned name (str()) directly as the event ctx, with no ctx struct. The event func returns false where the Lua returns nil (HAND_PLAYED, all_unlocked, F_NO_ACHIEVEMENTS), so the event stays queued as in the Lua.
- The chip_score unlock passes the prototype Table* directly as ctx through the two nested immediate events.
- `G.FILE_HANDLER.force = true` is tsetb(G->FILE_HANDLER, ...) right after Game_save_progress, which creates FILE_HANDLER in the Lua. This relies on the C Game_save_progress doing the same.
- Places where the Lua would crash on nil are not reproduced as errors: missing career stat, NULL G.DISCOVER_TALLIES, missing unlock_condition.extra. They read as 0/NULL through tnum/ttab, so the comparison just evaluates on 0 or NaN. The one exception is win_no_hand with a missing G.GAME.hands[extra], which raises ERROR().
- pairs() over G.playing_cards, G.jokers.cards, used_vouchers, P_CENTERS and P_BLINDS is counting only, so iteration order does not matter.
- The Brainstorm check keeps the two Card_get_id calls per card, because a Stone Card's id is a new random number on each call.

## common_events_5.c

generate_card_ui (common_events.lua:2397) is translated in full.

Notes:
- TAB(..., "key", NIL) is assumed to behave like tset with NIL (key absent). Possibly-nil values are passed straight through, e.g. "nodes", tget(full_UI_table, "name") and "vars", tget(_c, "vars").
- full_UI_table.name is stored as a Val: absent, B(true), or T(array of UI nodes). Callers must read it with tget, not ttab.
- Colours inside vars.colours and UI configs are P(float *); a nil colour is NIL, never P(NULL).
- loc_vars arrays are built with tseti per element, so a nil element stays a hole as in the Lua constructor.

Places where the C differs from the Lua on bad data:
- Where the Lua would raise "attempt to index nil" on missing data (_c.unlock_condition.extra.count, _c.config.extra.destroy, career_stats), the C reads NIL instead of aborting.
- The one exception is common_events.lua:2734 (a 'perishable' badge with nil specific_vars), which raises ERROR as the Lua does.
- For Planet cards, G.C.HAND_LEVELS[min(7, level)] gives NIL when the level is negative or not an integer (the Lua gives nil there too).

The two identical "any negative joker and remove_negative text exists" loops (Invisible Joker at :2531, Ankh at :2632) share two static helpers; behaviour is the same, including the break on first match.

## common_events_1.c

Translated common_events.lua:1-578 (set_screen_positions through update_hand_text; eval_card at :580 belongs to the next part). 20 header functions: the 18 Lua globals plus ease_value_f and ease_value_n.

Notes:
- Colours in tables: a static helper colour_val() reads P(float *) as the headers say, and also accepts a Lua-style {r,g,b,a} table (allocating 4 floats). It is there because data.h says game.dat stores colours as tables, so P_BLINDS[*].boss_colour may not have been converted by the loader. If the loader does convert, the table branch is dead.
- ease_background_colour_blind: the colours from mix_colours / darken / lighten go into local Colour buffers instead of being allocated, because ease_colour and ease_background_colour read them before returning. Behaviour is the same.
- update_hand_text: cover_colour for attention_text is allocated (mix_colours with out = NULL), because attention_text's args outlive the call. config and vals are captured by reference, as in the Lua.
- `<UIElement>.config.object:update()` (the Lua passes no dt) is CALL(object, update, 0.0f).
- ease_value*: ease_func is ease_floor when floored, otherwise NULL (event.h's identity). `type = ease_type` is not set, as event.h requires. The three variants share one static helper.
- ease_background_colour: iterates L, D, C in game.h's struct order and uses ease_value_f on the channels and on &G->C.BACKGROUND.contrast.
- level_up_hand: a Tag is juiced with Tag_juice_up after is_a(card, &Tag_class); anything else with CALL(card, juice_up, 0.8f, N(0.5)). G.TAROT_INTERRUPT_PULSE = nil is written as false.
- add_joker: card.created_on_pause = nil is written as false (bool member). G.P_CARDS.empty is passed as ttab(G->P_CARDS, "empty"), which is NULL.
- draw_card: the flipped_cards roll is pseudorandom(N(pseudoseed("flipped_card", NULL)), NIL, NIL) inside the event, as in the Lua. `from == G.deck ...` are pointer comparisons, so a NULL `from` matches a NULL area just as nil == nil does.
- G.ROOM.jiggle is read and written in G->ROOM->object.dyn, per game.h.
- Text built with `..` on numbers uses number_tostring (%.14g).

## misc_functions_4.c

No functions were written, deliberately: the file holds only #include "all.h" and a comment. Lines 1891-2022 are the seven main-thread sound fallbacks (PLAY_SOUND, STOP_AUDIO, SET_SFX, MODULATE, RESTART_MUSIC, AMBIENT, RESET_STATES) for G.F_SOUND_THREAD = false. misc_functions.h declares no prototype for this range and says misc_functions.c must not define them (misc_functions.h:19-22, io.h:13-17, `docs/header-notes.md` "Sound functions not declared here"): io.h declares the same seven names as the engine/sound_manager.lua versions, defined in src/sound_manager.c only, so defining them here would be a duplicate symbol at link time. If the fallback copies were ever wanted, they would need different names, because the logic differs from the thread version (the fallback always creates a new source and its MODULATE does not run AMBIENT). The game.lua:1524 call RESET_STATES(G.STATES.MENU) must stay a comment at its call site per io.h; that is outside this file.

## common_events_2.c

All 9 functions of lines 580-1162 are translated (eval_card, set_alerts, set_main_menu_UI, card_eval_status_text, add_round_eval_row, change_shop_size, juice_card, update_canvas_juice, juice_card_until).

Notes:
- **Nested tables**: every nested table inside TAB/ARR is wrapped in T(...) (TAB's values are Val), not bare TAB.
- **set_alerts storage**: G.ARGS.set_alerts_alertables is kept in G->ARGS under that key (game.h lists it there), not as a file static as common_events.h suggests. The alert boxes G[v.alert_uibox_name] are O(UIBox) in G->object.dyn; that includes your_collection_boosters_alert, which game.h's dyn list omits (it names ten, the Lua has eleven).
- **set_alerts and OVERLAY_MENU_TRUE**: G->OVERLAY_MENU is dereferenced whenever it is non-NULL, as the Lua does. If REFRESH_ALERTS is ever set while it holds OVERLAY_MENU_TRUE, this crashes (the Lua would error too).
- **eval_card comparisons**: the area tests use veq(tget(context, "cardarea"), O(G->play)) and so on, so nil == nil stays true as in the Lua. The third block compares context.card with G.consumeables, as the Lua writes it. Card_get_edition takes no context in card.h, so none is passed.
- **card_eval_status_text**:
  - `text` is the Val localize returns (S, or T for a multi-line entry) and goes to attention_text as is.
  - The 'dollars' text uses number_tostring(fabs(amt)).
  - config.type and extrafunc are dead in the Lua and not materialised.
  - card.jimbo is read from the card's dyn; G.ROOM.jiggle from G->ROOM->object.dyn.
  - The instant path and the queued event share one static body, status_text_show.
- **juice_up calls**: every card:juice_up goes through CALL(card, juice_up, ...), the class slot, so a Card_Character's card or another Moveable works.
- **add_round_eval_row**:
  - One shared context (config, width, num_dollars, dollar_row) is used by all its events. dollar_row is incremented when the dollar events run, as with the Lua's upvalue; each per-dollar event has its own small context holding i.
  - G.round_eval.divider_added is in the UIBox's dyn.
  - A non-number config.pitch on a non-'bottom' row raises ERROR (the Lua's arithmetic error on `1.5*config.pitch`), after the 'cancel' sound, in the same order as the Lua.
  - The 'joker' row passes localize{type='name_text'} unwrapped as the DynaText string, as the Lua does (DynaText_init wraps a lone value).
  - The unused local dollar_string (:1023) is not computed.
  - The larger UI definitions are built through named local tables (same shape and keys) to keep the nesting readable.
- **update_canvas_juice**: computed in double and stored into the float members. eased_cursor_pos is the Table in G->ARGS, created here if missing. `G.CURR_VIBRATION or 0` relies on the member being 0 as nil. The rumble call goes to love.h's Joystick_setVibration, currently a no-op.
- **juice_card_until**: eval_func is called with fcall(fn, 1, &O(card)); the requeue passes first = false and delay 0.8.

## misc_functions_1.c

All 31 functions of lines 1-626 are written.

Notes:
- timer_checkpoint (:66-102): the perf overlay body is not translated (debug-only, uses collectgarbage). If G.F_ENABLE_PERF_OVERLAY were ever true it returns without recording anything.
- pseudoseed: the "%.13f" round trip uses libc snprintf + strtod, as the header prescribes. The PS2 libc may round doubles wrongly; the spot is the static helper pseudoseed_round13, marked with a `ponytail:` comment, so an integer version would replace only that helper. Not verified against PC vectors.
- generate_starting_seed: cursor_hover.T is a float Vec2, so the entropy seed differs from the Lua's doubles. It is only entropy, not parity.
- sort_id lookup (pseudoshuffle, pseudorandom_element): reads Card.sort_id for a Card, obj.dyn.sort_id for another object, t.sort_id for a table. "type(v) == 'table'" is treated as table or object.
- pseudorandom_element: mixed number/string keys and an empty table are ERROR, as the Lua fails. String keys compare with strcmp.
- remove_all: assumes every value is O(Node). The second loop iterates the live table with titer while each node's remove may clear its own entry; this needs titer to tolerate removal during iteration, as Lua's next does. A non-object value there is ERROR.
- GET_DISPLAYINFO: love.h exposes a single fullscreen mode through out-pointers, so the :40 loop runs over that one mode. G.SETTINGS.WINDOW.DISPLAYS must already exist, as in the Lua.
- boot_timer: love_graphics_rectangle has no corner radius, so the Lua's radius 5 is dropped. G.LOADING is TAB("font", P(Font *)).
- HEX: returns 4 malloc'd floats; a string with no run of 8 hex digits is ERROR (the Lua's tonumber(nil, 16)).
- get_blind_main_colour: expects boss_colour stored as P(float *) in G.P_BLINDS entries (an open point of misc_functions.h in `docs/header-notes.md`).
- evaluate_poker_hand: the downgrade steps :507-517 copy groups, not cards, leaving holes as the Lua does. get_X_same makes the same number of Card_get_id calls in the same order as the Lua (Stone cards draw from the generator).
- get_X_same / get_straight: Card_get_id returning 0 (a card with no rank) is not special-cased; the Lua would have nil there.

## common_events_4.c

All 23 top-level functions of common_events.lua:1772-2396 are translated (notify_alert through get_type_colour).

Notes:
- **G.ARGS.TEMP_POOL**: get_current_pool keeps it in `G->ARGS` under "TEMP_POOL" (as game.h lists it), not in a static as common_events.h's top comment suggests. It is still the same table on every call.
- **Debug values**: G.FORCE_TAG and G.FORCE_BOSS are left as one-line comments, as common_events.h says, even though game.h has the members.
- **inc_steam_stat**: empty body with a comment (Steam only).
- **get_new_boss**: the last pairs loop clears entries while iterating in the Lua; the C collects the keys first and clears them afterwards (same result). An eligible boss missing from G.GAME.bosses_used raises ERROR, where the Lua fails comparing a boolean with a number.
- **get_current_pool**: a `_type` with no pool raises ERROR (the Lua's ipairs(nil)).
- **create_card**: params.bypass_discovery_ui is left absent (nil) rather than false when G.shop_demo is NULL and the area is not a shop or pack area, matching the Lua's `or` chain. The other three params are always set.
- **tutorial_info**:
  - overlay_colour is 4 malloc'd floats per call, never freed, eased with `ease_value_f(&colour[3], ...)`.
  - The G.OVERLAY_TUTORIAL fields (step, step_complete, Jimbo, content, highlights, button_listen, skip_steps) live in its dyn table.
  - As in the Lua, a removed `content` box stays referenced in dyn until a later step replaces it. If UIBox_remove frees the object, a second remove on a later step without content would touch freed memory.
  - Jimbo's button is added with `yoff = NAN` (nil).
- **get_type_colour**: the `{0, 1, 1, 1}` fallback is a static array, not a fresh table; the `discoveredand` typo is kept.
- **unlock_notify**: the profile part of the path is `vstr(G.SETTINGS.profile)`; the split on "\n" also yields empty lines and the last one, as the Lua's gmatch does.
- **File-local helpers**: ante_str, banned, used_no_showman, pack_weight and non_stone_cards are static; non_stone_cards is the identical filter loop shared by reset_idol_card, reset_mail_rank and reset_castle_card.

## state_events_3.c

FUNCS_tutorial_controller (:1210) and FUNCS_tutorial_part (:1281) are translated in full.

Notes:
- **Closures as FN values:** `snap_to` and the lazy `highlight` are `FN(f, ctx)` called with zero arguments. `snap_to` returns `O(node)` or NIL; `highlight` returns `T(array of O(node))`. This matches the `tutorial_info` comment in common_events.h.
- **Shared closures:** identical Lua closures share one static function (s_3/s_4, s_9/s_10, and the two `G.hand.cards[1]` snaps). The two blind-select snaps and the two shop_jokers snaps take their index through ctx as `(void *)(intptr_t)n`.
- **`attach` tables:** built by a static helper as `{major = O(node), type = S, offset = {x, y}}`, the shape `Moveable_set_alignment` takes.
- **Nil entries in eager highlight arrays:** a missing card (e.g. `G.shop_jokers.cards[2]`) or an id that is not found becomes `O(NULL)`, i.e. NIL, inside `ARR(...)`. This assumes `arr_of` accepts NIL as a hole or a shorter array, as the Lua constructor does.
- **Lua errors become NULL dereferences:** where the Lua would raise on indexing nil (`G.blind_select.UIRoot.children[1].children[n]`, `get_UIE_by_ID(...).parent`), the C dereferences NULL. No guards were added, to keep the Lua's conditions.
- **`G.OVERLAY_TUTORIAL` fields:** `step`, `step_complete`, `skip_steps`, `Jimbo` and `content` are read from the UIBox's dyn, as game.h says. Jimbo is removed with `Card_Character_remove`, and content and the overlay with `UIBox_remove`.
- **Final event:** `.not_blockable = true`, `.timer = "REAL"`, queue "tutorial". Its ctx holds the final `step` and the `_part` pointer without copying it; the only caller passes string literals.
- **No registration here:** neither function is referenced by string, so this file calls no `funcs_register`.

## state_events_1.c

Written: win_game, end_round, new_round, FUNCS_draw_from_deck_to_hand, FUNCS_discard_cards_from_highlighted, FUNCS_play_cards_from_highlighted, FUNCS_get_poker_hand_info, plus 17 static event closures and 3 static helpers.

Notes:
- **Registration is not done here.** state_events.h says state_events.c registers 'play_cards_from_highlighted' and 'discard_cards_from_highlighted', but no start-up function is declared for this file. It relies on funcs_register_all (button_callbacks.h), which already registers both, discard through its hook=false wrapper.
- **Globals mult / hand_chips are not defined here.** Their Lua definition is at lines 615-616, so they belong to the next part of state_events.
- **Symbols expected from the other parts of state_events:** FUNCS_evaluate_play, FUNCS_draw_from_play_to_discard, FUNCS_draw_from_hand_to_discard, FUNCS_draw_from_discard_to_deck.
- **Most-played hand tie-break (end_round :131).** pairs(G.GAME.hands) is walked in insertion order, not LuaJIT hash order. Because the Lua never updates _order, the last tied hand visited wins, so ties may pick a different hand than the PC game (this feeds The Ox). Marked with a `// note:`.
- **win_game, G.I.CARD loop.** sticker_run is cleared as the Card member for Cards, and as a dyn key for the Blind and Card_Characters that share the list.
- **win_game, Jimbo upvalue.** It is passed by value to the wishlist event, which is queued after the assignment.
- **end_round juice event (:216).** It captures the effects table and the index and reads effects[ii].card when it runs, as the Lua closure does, then calls juice_up through CALL.
- **Discard, card_area_focus_reset.** When the focused target is a Card, its area and rank members are used; for any other node they are read from its dyn. The field is never read by the game.
- **Number-to-string concatenations.** 'nr'..ante and ante..blhash use vstr on the ante value, which formats as Lua's tostring.
- **Nil conventions used.** hard_set_T() is four NANs; nil percent, delay and vol are -1; a nil amt to card_eval_status_text is 0.
- **Non-table joker results.** A truthy calculate_joker result that is not a table (the Lua would fail indexing it) is passed on as NULL through the static vtab helper.

## misc_functions_2.c

36 public functions (nuGC .. set_hand_usage). copy_table (lua:979) is the header's static inline, not defined here.

Notes:
- play_sound / modulate_sound call sound_manager_request unconditionally (no G.F_SOUND_THREAD branch), as io.h and `docs/header-notes.md` ask; the main-thread PLAY_SOUND/MODULATE fallback does not exist in the port.
- Two-argument math.log (number_format's e-branch base 10, modulate_sound's organ base 5) uses the LuaJIT 2.1 form log2(x) * (1/log2(b)) via a static lj_log, same form as lj_log10b in button_callbacks_3.c. This form was not verified against the shipped lua51.dll when written: check number_format(1e12), (1e15), (1e23) on PC.
- number_format relies on the platform's printf for "%.4g" / "%.Nf" and on core's tonumber for the string round trip (half-to-even rounding on the PS2 libc is the thing to check). For a truthy value that is neither number nor string it returns tostring(v); the Lua returns the value itself.
- prep_draw uses the moveable's own layered_parallax always. A Vec2 cannot be nil, so the Lua's fallback to parent.layered_parallax is not there. That matches card_1.c, which stores {0,0} for the card's front/back/center sprites; it would differ only if a card area ever had a non-zero parallax.
- set_consumeable_usage calls discover_card(card->moveable.node.object.dyn): the Lua passes the Card itself, not its center, so discovered / wip / alert are read and written on the card's dyn table. Kept literal.
- set_consumeable_usage's nested events keep the Card pointer as ctx and read config.center_key when the inner one runs, as the Lua closure does.
- G.ARGS.ambient_sounds is a real table in G->ARGS: four {volfunc = FN} entries sharing one ctx with the dt of the first modulate_sound call (the Lua closure latches it). pairs() order there and in get_deck_win_stake's _w_low is insertion order, not Lua hash order (noted in the code).
- find_joker dereferences G->consumeables without a check once G->jokers exists, like the Lua.
- get_blind_amount raises ERROR for a scaling other than nil/1/2/3, as the header documents.
- send_score / send_name keep the flag test with an empty body (HTTP and Steam are not translated). nuGC only applies its defaults (no collector).
- wins[G.GAME.stake] and G.sticker_map[_w] use tgetv/tsetv with N(); core normalises integer-valued keys to the array part.
- File-local statics: cur_profile, lj_log, new_usage, selected_back_center, the four volfunc_*, and four event callbacks.

## state_events_2.c

The 7 header functions of state_events.lua:571-1209 are translated (FUNCS_evaluate_play, the five FUNCS_draw_from_*, FUNCS_evaluate_round).

- Globals: `num mult` and `num hand_chips` (declared extern in state_events.h) are DEFINED in state_events_2.c, since their first assignment is at :615-616. state_events_1.c and state_events_3.c do not define them at the moment; they must not, or the link gets duplicate symbols.
- Context value encoding: cardarea = O(CardArea *), full_hand = T(G->play->cards) (the area's own table), scoring_hand / poker_hands / removed / card_effects = T(table), other_card / other_joker / destroying_card = O(Card *), flags = B(true). Colours in the `extra` tables are P(G->C.EDITION) / P(G->C.DARK_EDITION).
- update_hand_text vals: `extras.x and value` is passed as B(false) or N(value), as the Lua does; `cond and v or nil` keys are simply absent.
- card_eval_status_text: the Lua's nil amt is passed as 0 and nil percent as -1 (the callee then makes the math.random call), per common_events.h. `percent` is kept as a double and narrowed to float at each call; it only affects sound pitch.
- Closures at :838 (juice of effects[ii].card) and :987 (destroy cards_destroyed[i]) capture the table and the index and read the card when the event runs. :838 uses CALL(card, juice_up, 0.7f, NIL).
- The edition `message` and every `localize{type='variable',...}` result are kept as the Val that localize returns.
- Lua-error paths not reproduced (unreachable in practice): G.GAME.hands['NULL'] when no hand matched, and a non-empty repetition eval lacking .seals / .jokers (:671, :825), which in C reads 0 repetitions.
- card.shattered / destroyed / lucky_trigger live in the card's dyn (card.h); debuff is a member; blind.triggered / chips / dollars are Blind members.
- The local `hand_text_set` (:627) is kept, unused as in the Lua.

## UI_definitions_1.c

Notes:
- **`create_UIBox_highlight` is weak.** The Lua defines it twice, identically (:463 and :6302). This one is `__attribute__((weak))`, the same pattern as `ui_ref_get` in `ui_1.c`, so a strong definition from the file holding :6302 wins and the link works either way.
- **`UIDEF_card_focus_ui_set_alignment(Card *)`** is the :325 closure. Nothing is stored on the UIBox; `Card_align` in `card_6.c` already calls it. It passes `card.children.focused_ui` straight to `Moveable_set_alignment`, with no NULL guard, as the Lua does.
- **`playing_card_colour`** is a malloc'd 4-float block that is never freed (it belongs to the definition). Its alpha is eased with `ease_value_f(&colour[3], -1.5, false, "REAL", -1, 0.2f, "quad")`.
- **Conditional node entries** are written as `ARR(cond ? T(...) : NIL, ...)`. This relies on `arr_of` keeping a leading NIL as a hole and trimming trailing ones. Empty `nodes = {}` are `tnew()`.
- **`UIDEF_deck_preview`:**
  - It calls `remove_nils(G->playing_cards)` and discards the result, as the Lua does, then `tsort`s `G->playing_cards` in place.
  - `stones` nil versus 0 is a `has_stones` flag.
  - `rank_counts` and `SUITS[suit][id]` are indexed with `(int)base.id` through `tgeti` / `tseti`.
  - `pairs(SUITS)` and `pairs(suit_counts)` run in insertion order; the order does not affect the result.
- **`UIDEF_card_focus_button`** with an unknown type leaves `button_contents` as an empty table, as the Lua does.
- **Geometry is computed in float** (card widths, offsets) and stored with `N()`.
- **Localized text concatenations** (`localize(..)..': '`, `' '..localize{type='variable',..}`) use `vstr()` of the result. A table result, which is a Lua error, would print "(null)".

Not ported:
- UI_definitions.lua:5 create_UIBox_debug_tools: debug only (and its 16 G.FUNCS.DT_*), body is TODO_PORT as UI_definitions.h prescribes
- UI_definitions.lua:196 create_UIBox_online_high_scores: HTTP manager, body is TODO_PORT as UI_definitions.h prescribes
- UI_definitions.lua:206 create_UIBox_high_scores_filling: HTTP manager (loadstring of the response), body is TODO_PORT as UI_definitions.h prescribes

## misc_functions_3.c

All 25 header-declared functions of misc_functions.lua:1259-1890 are translated.

Notes:
- **get_stake_sprite, stake 8**: the sprite gets a file-private `static Class stake8_sprite_class` (name "Sprite", super `&Sprite_class`, draw replaced). It is not in `classes_init`; it is `class_init`-ed lazily on first use. Its `send_to_shader` is one static scratch table shared by all stake 8 sprites, so nothing is allocated per frame.
- **save_run**: `pairs(G)` for card areas is `G_CARDAREA_NAMES`, plus `title_top`, plus `G->your_collection` when it is a single CardArea, plus any CardArea in `G->object.dyn`.
- **recursive_table_cull**: `V_OBJ` becomes the string `"MANUAL_REPLACE"` (with quotes), `V_TAB` recurses, everything else is copied as is. A colour stored as `P(float *)` is therefore copied as a pointer, where the Lua would copy its four numbers. If any such pointer lives inside `G.GAME`, `STR_PACK` has to cope with it.
- **localize**: where the Lua would raise, the C calls `ERROR()`: nil `args`, an unknown `set` in `descriptions`, nil `args.vars` or `vars.colours` when a part needs them, nil `args.nodes`, and concatenating a nil variable in type `variable`. Type `name_text` reproduces the pcall and returns "ERROR" instead. Scales are stored as `N(double)`. The DynaText config keys are the Lua's, including `"float"`.
- **loc_parse_string and localize** build strings in local buffers and intern once with `S()`, rather than `fmt` per character, so no garbage is left per step. A whitespace byte under an `X` control still creates the empty string part, as the Lua does.
- **convert_save_to_meta** is translated in full although a console never needs it. It uses `get_compressed`, `love_filesystem_getInfo`/`remove`, and `compress_and_save(file, STR_PACK(_meta, false))`.
- **dec_stop_use** passes `_depth` in the event ctx as an `intptr_t`, with no allocation; both events use `.not_blocking = true, .no_delete = true`.
- **set_discover_tallies** calls `check_for_unlock` unconditionally, since the function always exists in C.
- **remove_save** calls `ERROR` if `G->FILE_HANDLER` is NULL, which is the Lua's index-nil error.
- **utf8_next** returns each character through `str_sub`, so the strings are forever-lived as core.h defines.
- **Profile paths** use `vstr(G.SETTINGS.profile)`, i.e. the number formatted as Lua's tostring does.

## UI_definitions_3.c

All 12 functions of UI_definitions.lua:1274-1902 are translated.

Functions: create_UIBox_HUD, create_UIBox_blind_select, create_UIBox_blind_tag, create_UIBox_blind_choice, create_UIBox_round_evaluation, the five create_UIBox_*_pack, create_UIBox_card_alert, create_slider.

Notes:
- The five pack functions share one static body, pack_ui(width_mult, width_add, title_key); the Lua bodies differ only in the area's width and the title key.
- Every object a definition creates (DynaText, Sprite, Moveable, UIBox, CardArea, Tag) is built in a local first, in the Lua's order, because C does not fix argument evaluation order. The three create_UIBox_blind_choice calls run in the order Small, Big, Boss (RNG 'orbital').
- File-local macros UI(t, cfg, nodes) and UI0(t, cfg) expand to the literal TAB("n", ..., "config", ..., "nodes", ...). Empty `nodes={}` is T(tnew()), since ARR() cannot take zero arguments.
- Conditional entries of `nodes` are holes set with tseti at their own index (the reroll button at 3, the blind select options at 1..3, the debuff text rows, the reward row).
- G.GAME.orbital_choices[ante] is keyed with the Val of round_resets.ante through tgetv/tsetv, so it is a number key.
- The blind choice's ref_table {deck = nil, run_info = run_info} holds run_info = B(true) only when true; false is stored as absent.
- `disabled` is kept as a const false in create_UIBox_blind_choice and create_UIBox_blind_tag, with the Lua's ternaries intact.
- create_slider reads ref_table[ref_value] through ui_ref_get, so a table or an object works. args.text is fmt("%.*f", (int)decimal_places, value). `min_h` keeps the Lua's spelling.
- create_UIBox_card_alert reads args.bg_col as a P(float *) colour.
- G.blind_select_opts is a new table with small/big/boss = O(UIBox), absent when that blind is hidden.
- The event at :1432 is queued before the blind choice boxes are built (immediate, default flags, base queue).

## UI_definitions_6.c

Translated the 12 header functions whose definition starts in 3183-3808: UIDEF_current_stake, UIDEF_view_deck, tally_sprite, UIDEF_used_vouchers, create_UIBox_your_collection and its _jokers, _tarots, _boosters, _planets, _spectrals, _vouchers and _seals pages.

Notes:
- Evaluation order: C does not fix the order of TAB/ARR arguments, so everything with side effects inside a definition (Sprites, DynaTexts, UIBoxes, UIBox_button, create_option_cycle, tally_sprite) is built in locals first, in the Lua's textual order, and the tree is assembled afterwards. This keeps node ID and G.I registration order the same as the Lua.
- File-local helpers (all static): COL (float* to P(), NULL to NIL), room_x, tally_pair, page_label, view_deck_less (sort comparator), ev_refresh_alerts (event at :3501, queued with .not_blockable = true).
- UIDEF_view_deck:
  - rank_tallies / mod_rank_tallies are C arrays indexed 1..13. A card id outside 2..14 raises ERROR, where the Lua would fail on nil arithmetic.
  - The suit tallies are Tables keyed by suit name, as in the Lua. A card whose suit is not one of the four raises ERROR at :3246, like the Lua's table.insert on nil.
  - remove_nils' result is discarded, as the Lua does.
  - copy:hard_set_T() is Card_hard_set_T with four NANs, per card.h.
  - selected_back is read as tobj(G->GAME, "selected_back") and challenge as tstr(G->GAME, "challenge").
- Text values: T-node 'text' is N() where the Lua gives a number (rank names 2..10, rank_tallies[i]) and S() otherwise. The ''..n strings use number_tostring.
- Nil holes are kept at their index: other_col in current_stake's root nodes, the two optional legend rows in view_deck, and the second DynaText string when not modded.
- G->your_collection is T(array of O(CardArea)) in the paged screens and O(CardArea) in the seals screen, per game.h.
- The spectrals page count uses the Tarot pool for the loop and the Spectral pool for the label; this Lua quirk is kept.
- card.sticker for jokers is get_joker_win_sticker(center, false), stored as its string or NULL.
- G.P_CARDS.empty is passed as ttab(G->P_CARDS, "empty"), which is NULL as in the Lua.

## UI_definitions_2.c

All 17 header functions whose definition starts in UI_definitions.lua:637-1273 are translated.

Functions: UIDEF_shop, create_card_for_shop, create_shop_card_ui, attention_text, create_UIBox_buttons, desc_from_rows, transparent_multiline_text, info_tip_from_rows, overlay_infotip, name_from_rows, UIDEF_card_h_popup, get_badge_colour, create_badge, create_UIBox_detailed_tooltip, create_popup_UIBox_tooltip, create_UIBox_HUD_blind, add_tag. The five event closures are static (ev_shop_sign, ev_shop_card_tags, ev_shop_card_ui, ev_attention_text_show, ev_attention_text_fade).

Notes:
- File-local helpers: macros NODE(uit, cfg, nodes) (expands to the plain TAB("n", ..., "config", ..., "nodes", ...); nodes NULL means no `nodes` key) and LOC(key); static colour_copy / colour_new for copy_table(colour) and literal colours.
- Argument evaluation order: UI trees are nested TAB/ARR calls, and C does not fix the order in which arguments are evaluated. In create_UIBox_HUD_blind the two DynaTexts are hoisted into locals to keep the Lua's creation order. In create_card_for_shop the first 'illusion' roll sits inside the ARR(...) that builds the rates table; it is the only side-effecting argument there, so the RNG order is the Lua's (cdt, then illusion 0.6, then create_card, then illusion 0.8 and the edition roll).
- attention_text: cover_colour_l / cover_colour_d use the new colour that lighten / darken already return, without the Lua's extra copy_table. The stray positional args.scale in the DynaText config is kept (index 1). args is the ctx of both events and must outlive the call, as the header says.
- UIDEF_card_h_popup: for a Card, debuff and ability are the struct members; for a tag's Sprite they are read from its dyn. get_type_colour gets the Card or NULL. The card-type badge text goes through vstr, so a multi-line (table) name_text would show 'ERROR' because create_badge takes a const char *. The name row may be absent: nodes index 1 is left unset and indices 2 and 3 are kept.
- 'cdt'..ante is built with vstr of the ante, i.e. the number as tostring writes it.
- The Moveable() of the show_infotip node is NEW(Moveable) + Moveable_init_args(m, NULL), the same as in the other files.
- add_tag: if a 'tag_add' handler removes a tag during the loop, the Lua errors on nil; the C passes NULL to Tag_apply_to_run (the count is taken once, as in the Lua).

## UI_definitions_5.c

All 17 functions whose definition starts in 2545-3182 are written (UIDEF_custom_deck_tab through UIDEF_deck_stake_column).

Notes:
- **Function values:** the tab functions (view_deck, current_hands, current_blinds, used_vouchers, current_stake) and the tooltip filler (create_UIBox_hand_tip) are stored as FN(static wrapper, NULL), following the one-argument contract in UI_definitions.h. The toggle callback in custom_deck_tab is FN(FUNCS_refresh_contrast_mode_fn, NULL).
- **Evaluation order:** where a Lua list holds several calls that create objects (DynaText, Sprite, rows), the entries are built with statements in the Lua's order instead of nesting them as arguments of ARR/TAB, because C does not fix argument order. UIDEF_current_blinds is the case that matters for rules: create_UIBox_blind_choice may poll 'orbital'.
- **nil holes:** conditional entries are set at their own index with tseti, or passed as NIL inside ARR.
- **create_UIBox_demo_video_CTA:** RESTART_MUSIC(NULL) is called as the Lua does (nil args); per io.h this raises ERROR, so the TODO_PORT after it is normally not reached. Demo only.
- **Dead branches kept:** show_win_cta (a never-assigned Lua global) and show_lose_cta are local constants set to false, with both branches translated.
- **Eased colours:** eased_green / eased_red are new 4-float blocks, eased with ease_value_f(&c[3], mod, false, NULL, 1, -1, NULL).
- **create_UIBox_hand_tip:** the ease holds &card->T->scale for 0.2 s (REAL timer); the pointer dangles if the tooltip's cards are freed sooner (the event.h lifetime caveat).
- **pairs order:** the hand_usage walks (profile and run) use insertion order; ties in count may resolve differently from Lua's hash order. Marked with `// note:`.
- **custom_deck_tab:** the keys of localize(_suit, 'collabs') are converted with tonumber(it.key), so it works whether the data loader stored "1" or 1.
- **Small departures from the Lua:**
  - create_UIBox_round_scores_row('hand') with no G.GAME.round_scores.hand shows an empty amount where the Lua would error.
  - In create_UIBox_current_hand_row, a level that is negative or not an integer gives a nil colour (the Lua's HAND_LEVELS has only 0..7).
  - Numeric locals for UI sizes (text_scale, label_w and similar) are num, not float, to match the Lua's arithmetic; they only go into N().
- **Static helpers in the file:** cur_profile, colour_copy, floor_percent, dyna_node, ui_atlas, jimbo_spot, progress_filling (the four identical `filling` lists of create_progress_box), hand_row_tooltip.

Not ported:
- UI_definitions.lua:2850 create_UIBox_demo_video_CTA: love.graphics.newVideo and everything after it (the video sprite, its volume, play(), the generic options box around it) - love.h has no video; demo only. The event at :2836 and RESTART_MUSIC() at :2848 are translated before it.

## UI_definitions_4.c

All 14 header-declared functions of UI_definitions.lua:1903-2544 are translated.

Notes:
- **File-level state:** `UI_global_type` and `UI_global_set` (the Lua's accidental globals `_type` / `_set`, :2438) are DEFINED in this file, as UI_definitions.h asks.
- **Function values:** the `tab_definition_function` wrappers are statics here: `settings_tab_fn`, `usage_fn`, `custom_deck_tab_fn` (which calls `UIDEF_custom_deck_tab` from another part) and `tabs_sample_fn` for the three sample tabs. The high-contrast toggle's callback is `FN(FUNCS_refresh_contrast_mode_fn, NULL)`.
- **Order of side effects:** in `UIDEF_settings_tab`, `create_tabs`, `create_UIBox_options` and `create_UIBox_usage`, every child that creates an object (Sprite, DynaText, UIBox, Card, CardArea) is built in its own statement in the Lua's order, because C does not order call arguments.
- **nil holes:** conditional entries are kept as holes at their own index through `ARR(..., NIL, ...)` (`arr_of` keeps them): the options menu contents, the Game tab's entries 3 and 8, the shoulder pips, the keyboard's side column.
- **One deviation from the Lua:** in `create_UIBox_usage` the bar height `count/max_amt*3.6` is 0 when `max_amt` is 0, where the Lua gives NaN. It is marked with a `// note:`.
- **Lua errors kept as `ERROR()`:** `create_tabs` with no chosen tab, `create_UIBox_usage` when the profile has no table for the type, `create_text_input` when `ref_table[ref_value]` is nil, and the Video tab when `DISPLAYS[selected_display]` is nil.
- **`create_text_input`:** reads the bound value with `ui_ref_get`, so `ref_table = O(G)` (the run's seed) works; `min_h` is kept as the Lua spells it.
- **Video tab:** `res_option or 1` is written as `res_option ? res_option : 1`, since `GET_DISPLAYINFO` returns an int.
- **Colours:** copies and literals (`copy_table(colour)`, `{GREY rgb, 0.7}`, `{0,0,0,0.08}`, `{BLACK rgb, 0.6}`) are new 4-float blocks that are never freed, made by the static helpers `colour_new` / `colour_copy`.
- **Shared static helpers:** the shoulder-pip column and the info rows, which the Lua repeats verbatim in `create_option_cycle` / `create_tabs` and `create_toggle` / `create_option_cycle`, are single static helpers (`shoulder_pip`, `info_rows`) producing the same tables.

## UI_definitions_8.c

UIDEF_credits (UI_definitions.lua:4328) is the only top-level function starting in 4328-5205.

- The six tab_definition_function closures are static V_FN wrappers (credits_production_fn, _publishing_fn, _collabs_fn, _sounds_fn, _imagery_fn, _misc_fn) that ignore their argument and return T(ROOT). Their only upvalue, text_scale = 0.75, is a file-static const num.
- Deviation from literal line-by-line: the repeated node shapes are built by static helpers (txt, row, box, names, tab_root, sound_box, publishing_gap, collab) instead of being written out hundreds of times. Each call makes fresh tables with the Lua's keys in the Lua's order.
- Conditional entries are nil holes at their own index: the Instagram row (index 3 of the soundtrack column, set only with G->F_EXTERNAL_LINKS, so the next row sits at index 4) and the itch.io row (index 3, the last, set only with !G->F_BASIC_CREDITS).
- The Collabs closure creates G->collab_credits once, as name -> {artist, artist_link, ml_text, art} with nil fields absent. It collects the keys with titer, sorts them with tsort(t, NULL, NULL), and sets G->viewed_collab to the first.
- The Lua's Moveable() is NEW(Moveable) + Moveable_init(m, 0, 0, 0, 0); the O node's `id = nil` is simply not set.
- Callbacks are stored by name: 'louisf_insta' (button), 'CREDITS_check_collab' (func), 'change_viewed_collab' (opt_callback, the args registry).
- The source file is UTF-8 and the credit strings contain non-ASCII characters (©, curly quotes, accented names), so it must be compiled as UTF-8.

## UI_definitions_7.c

Functions (UI_definitions.lua:3809-4327): create_UIBox_your_collection_enhancements, _editions, _decks, _tags, _blinds, UIDEF_blind_sprite_hover, UIDEF_blind_sprite_stop_hover, create_UIBox_blind_popup, create_UIBox_card_unlock, create_UIBox_deck_unlock, UIDEF_multiline_credit_text, UIDEF_viewed_collab_option.

Notes:
- UIDEF_blind_sprite_class is DEFINED here (super AnimatedSprite_class, name "AnimatedSprite"), as UI_definitions.h says. The collection builder calls class_init on it if it is not ready; classes_init may also want to cover it.
- stop_hover is installed from the start, not on first hover as the Lua's misplaced `end` does (:4021). Before the first hover it writes hovering = false and hover_tilt = 0 in the sprite's dyn where the Lua leaves nil.
- The blind sprite's dyn holds float, hovering, hover_tilt and discovered; its config is replaced by {blind = T(v), force_focus = true}.
- Every object a definition builds inline (DynaText, Sprite, UIBox, Moveable) is created in a local first, in the Lua's order, because C does not fix argument evaluation order and these objects take IDs and join G.I.
- The 8 card_unlock events are one static function with a step number. card.ID is captured by value at queue time (it never changes afterwards), so an event that fires after the overlay closed does not touch a freed card.
- card_unlock raises ERROR where the Lua would crash: event 1 with G.OVERLAY_MENU nil, and any event with it equal to OVERLAY_MENU_TRUE.
- blind.boss_colour is read with UI_colour, so it must be P(float *) as misc_functions.h states. common_events_1.c also accepts an {r,g,b,a} table shape; if the loader leaves tables, the popup would treat boss_colour as nil.
- Conditional nodes keep their Lua indices: the blind matrix cell has the object at index 2 with optional B nodes at 1 and 3.
- tag_matrix and blind_matrix raise ERROR on more than 24 tags or 30 blinds (the Lua would index a nil row).
- pairs(G.P_TAGS) and pairs(G.P_BLINDS) are walked in insertion order and then sorted by order with tsort.
- G.your_collection is T(array of O(CardArea)) in enhancements and editions, and O(CardArea) in card_unlock.
- G.P_CARDS.empty is passed as NULL to Card_init.
- G.OVERLAY_MENU.joker_unlock_table lives in the UIBox's dyn; card[deck_center.key] = true lives in the card's dyn.
- The two em dashes in the credits text are written as UTF-8 escapes (\xe2\x80\x94).

## game_1.c

Game_init and Game_start_up are translated. Three static helpers: new_sprite, xy, clamp_texture_scaling.

Notes:
- **Game_class is defined here** (game.lua:2, super Object_class, no virtual methods). No other file should define it; object.c's classes_init already references it.
- **SOURCES is assigned here** (game.lua:64-73): a new table with an empty table per .ogg, written to the single `extern Table *SOURCES` of io.h. sound_manager_init() is called right after, at the thread-start spot (game.lua:87), and refills it as io.h describes.
- **The three managers are empty tables**, as game.h says. SAVE_MANAGER's thread:start(2) has no counterpart because requests run inline. The HTTP thread start under F_HTTP_SCORES (game.lua:122-124) is a comment only, since the HTTP manager is not ported.
- **The dead sound-load loop (game.lua:90, `while ... and false`) is kept literally**, using love_thread_getChannel("load_channel") and Channel_pop from love.h. It never runs.
- **shared_stickers and shared_seals are built one statement per sprite**, not with TAB(...). C does not order argument evaluation, and the sprites must be created in the Lua constructor's order so their Node IDs and G.I positions match.
- **Version checks use strcmp** (Lua string comparison). The 'NOPE' OS branch is translated but dead.
- **Settings overlay is shallow**, via tsetv over titer(settings_file), as the Lua does.
- **C.SUITS pointers alias SO_1 or SO_2** depending on SETTINGS.colourblind_option.
- **Directory listings are freed** with free() per love.h's comment; the array from love_joystick_getJoysticks is not freed, since love.h does not say who owns it.
- **F_RUMBLE** is tested as `!= 0` (0 as nil).

## UI_definitions_11.c

Translated create_UIBox_generic_options (:6308), UIBox_dyn_container (:6352), simple_text_container (:6360) and UIBox_button (:6376), plus the static infotip event func (:6317).

Notes:
- create_UIBox_generic_options: the infotip event keeps the caller's args table as its ctx and reads args.infotip when it runs, so args must outlive the call (as the header says). A non-table infotip is passed to overlay_infotip as NULL.
- If G->OVERLAY_MENU is still OVERLAY_MENU_TRUE when that event runs, the C calls ERROR; the Lua would fail there too (method call on a boolean).
- The default `contents` is a single T node used directly as the `nodes` list, with no colour key (G.C.UI.RED is nil), as in the Lua.
- With args.no_back the back button (index 2 of the inner C node's nodes) is simply not set.
- Colours built in place (the grey background with alpha 0.7, and {1,1,1,0.9} for the count text) are fresh malloc'd 4-float blocks stored as P(float *), never freed.
- Moveable() at :6347 is NEW(Moveable) + Moveable_init(m, 0, 0, 0, 0).
- UIBox_button writes its defaults back into the caller's args; `x = x or nil` turns a false into an absent key. button == the string 'nil' gives no button key. args.col is tested as strictly boolean true.
- Pass-through config values (shadow, one_press, id, ref_table, mid, back_delay, snap_back and the like) are copied as raw Vals, so a nil stays an absent key.
- The count text uses tostring() on tally and of; a nil there prints "nil" where the Lua would raise a concat error.
- Two file-local static helpers, v_or and new_colour, are defined in this .c only.

## game_2.c

Game_init_item_prototypes (game.lua:216) is fully translated.

Notes:
- **game.dat loading is private to this file.** No shared header exposes the game.dat table or its path, so a static `game_2_data()` calls `data_load("data/game.dat")` once and keeps the root table (marked with a `ponytail:` comment). The path is relative to the current directory. Other users of game.dat (globals, language, GAME, CHALLENGES) should end up sharing one loader with it.
- **Fresh tables on every call.** Each call takes `tcopy()` of the "prototypes" section, because the Lua rebuilds the tables on every call (profile or language change) and the code mutates them.
- **bl_ox.vars[1]** is replaced after loading with `localize(S("ph_most_played"), NULL)`, as data.h asks.
- **boss_colour** arrives from the data as `{r, g, b, a}` tables. They are converted here to `P(float *)` (4 malloc'd floats), the encoding misc_functions.h documents for a blind's boss_colour.
- **`pairs()` order** is `titer` order (source definition order), not Lua's hash order. This affects the pre-sort order of P_LOCKED (ties of `order` between jokers, vouchers and backs under the unstable sort) and the two pools that are never sorted, Edition and Default. There is a `// note:` in the code.
- **TESTHELPER_unlocks** is kept as a constant-false branch, as in the Lua.
- **Profile path:** `G.SETTINGS.profile..''` is `vstr()` of the profile number. meta.jkr is created with `love_filesystem_append(path, "return {}", 9)` when missing.
- The three identical discovered/alerted blocks (blinds, tags, seals) share one static helper, `meta_discovered_alerted`; the logic and order are unchanged.

## UI_definitions_9.c

All 12 header functions whose definition starts in 5206-5679 are translated (UIDEF_challenges, UIDEF_daily_overview, UIDEF_run_setup, FUNCS_false_ret, UIDEF_profile_select, UIDEF_profile_option, UIDEF_stake_description, UIDEF_stake_option, UIDEF_viewed_stake_option, UIDEF_challenge_list, UIDEF_challenge_list_page, UIDEF_challenge_description).

Notes:
- Tab functions: the four tab_definition_function values are static V_FN wrappers in this file (tabfn_challenges, tabfn_run_setup_option, tabfn_profile_option, tabfn_challenge_description_tab). UIDEF_run_setup_option and UIDEF_challenge_description_tab are expected from the other parts.
- UIDEF_run_setup: registers "false_ret" with funcs_register on every call, as the Lua assigns it. It calls FUNCS_can_continue(NULL) for the made-up element. _can_continue is a C bool, so the Lua's nil and false are merged in the tabs' `chosen`. The normalised from_game_over stays a Val (NIL / B(false) / B(true)) for no_back, no_esc and tab_definition_function_args.
- G.run_setup_seed = nil is tset(G->object.dyn, "run_setup_seed", NIL).
- UIDEF_challenges: assumes G->OVERLAY_MENU is a real UIBox when non-NULL. With OVERLAY_MENU_TRUE the Lua would error on the method call; the C would dereference (UIBox *)1.
- UIDEF_profile_option: the text-input callback (:5389) is a V_FN called with no arguments. It writes G->FILE_HANDLER.force right after Game_save_settings, so it relies on that call having created FILE_HANDLER. create_progress_box(progress, 0.5) is passed as smaller = true.
- UIDEF_stake_description: raises ERROR when the stake center is nil, where the Lua errors indexing nil at :5454.
- UIDEF_viewed_stake_option: reads the extern UI_global_type (the Lua global `_type`).
- UIDEF_challenge_list: back_id is 'challenge_list' (duplicate key, last wins). The queued event calls FUNCS_change_challenge_list_page with {cycle_config = {current_option = 1}}.
- UIDEF_challenge_list_page: the completed-icon Sprite is created just before the row's UIBox_button call, not after it as in the Lua. Harmless unless UIBox_button creates nodes.
- UIDEF_challenge_description: the three identical Card constructions are one static helper, challenge_card; `pinned` is applied only for jokers, as in the Lua. `{[v.edition] = true}` is TAB(tstr(v, "edition"), B(true)). Conditional nodes are NIL holes at their own index via ARR.
- Moveable() is NEW(Moveable) + Moveable_init_args(m, NULL), as UI_definitions_2.c does; UI_definitions_8.c uses Moveable_init(0, 0, 0, 0) instead.
- UIDEF_daily_overview is translated in full with math_random(), though G.F_DAILIES is never set.

## game_5.c

Game_start_run (game.lua:2018) is translated in full.

- **init_game_object is not in this file.** Its definition starts at game.lua:1862, in game_4.c; this file only calls Game_init_game_object(self).
- **FUNCS_use_card at game.lua:2205 disagrees with button_callbacks.h.** The Lua is use_card({config={ref_table=v}}, nil, true), so mute is nil and nosave is TRUE. The C calls FUNCS_use_card(NULL, v, false, true); the header comment says (NULL, card, false, false) for this site.
- **Mixed G.I.CARD list.** The ACTION replay and the playing_cards rebuild test is_a(v, &Card_class) before reading sort_id / playing_card, since the list also holds the Blind and Card_Characters.
- **Seed line (2164) is nil-safe.** The Lua indexes G.SETTINGS.tutorial_progress unguarded; in C a nil tutorial_progress reads as "not completed" and gives the "TUTORIAL" seed. There is a `// note:` at the spot.
- **saveTable.BLIND.name nil (2035) is an ERROR()**, as the Lua would fail on :gsub. The "%s+" pattern is hand-coded as "has any non-whitespace character".
- **pseudorandom loop (2167):** keys equal to 0 are collected first, then written, so the table is never written while iterated.
- **Deck build (2340):** iterates P_CARDS in titer order. The result does not depend on that order because the list is sorted afterwards. The sort comparator uses strcmp on the concatenated s..r..e..d..g key.
- **HUD boxes:** create_UIBox_HUD() / create_UIBox_HUD_blind() and the get_UIE_by_ID('row_blind') lookup are evaluated into locals before TAB(), to keep the Lua's order (C argument order is unspecified).
- **Background draw step (2282):** send entries use ref_float / ref_colour for the G struct members as game.h prescribes; spin_amount keeps ref_table = T(G.ARGS.spin), ref_value = "amount".
- **Spin event (2298):** returns false forever, as the Lua does; it is not no_delete, so clear_queue removes it.
- **G['load_'..k] (2313):** the three shop areas go to the Game members; any other name goes to G->object.dyn as "load_<k>". The Lua's print is a printf.
- **Old G.SPLASH_BACK is not removed here**, as in the Lua.

## game_3.c

All 12 header functions of game.lua:845-1372 are translated (the 11 Game_* methods plus create_UIBox_sandbox_controls).

Notes:

1. Game_set_language (game.lua:993): there is no loadstring, so G.localization comes from data_load("data/loc_<language>.dat"), the same relative path convention game_2.c uses for data/game.dat. A missing .dat is an ERROR (the Lua's assert). The two existence checks still test the Lua path "localization/<lang>.lua" through love_filesystem_read / love_filesystem_getInfo, as the Lua does. If the PS2 pack does not ship those .lua files, the language is forced to en-us and no localization is loaded at all; the pack must contain them, or those checks must be pointed at the .dat.

2. LANGUAGES, FONTS, the atlas lists and the default profile were built inline, not from game.dat, when the file was written (`docs/development-log.md`: the texts moved to the data later). The 48 collab atlas entries are generated by a loop over the 24 collab codes, giving the same names, paths, sizes and order as the Lua. Source strings are UTF-8 literals.

3. Game_set_render_settings allocates a new Atlas (calloc) for every entry on each call, like the Lua's new tables; old ones are not freed. Live sprites are rebound through CALL(sprite, reset). The path uses tostring(texture_scaling), so it reads ".../2x/..." and love_graphics_newImage maps it. The anisotropy argument of setDefaultFilter is dropped because the platform function takes two arguments.

4. Game_delete_run: the Lua's pairs(G) scan for CardAreas becomes: every name in G_CARDAREA_NAMES, plus title_top (not in that list but a CardArea member), plus your_collection when it holds a single CardArea. OVERLAY_TUTORIAL's Jimbo and content are read from its dyn table. focus_cursor_stack and focus_cursor_stack_level are written to the Controller's dyn, as controller.h says.

5. Game_prep_stage: the loop that nils every key of CONTROLLER.locks is tclear(locks). SETTINGS.paused is stored as false, not removed. ROOM.jiggle is in ROOM's dyn.

6. Sandbox (debug stage) is translated in full because game.h declares it. The eight G.FUNCS are statics registered inside create_UIBox_sandbox_controls, as the Lua assigns them at run time: funcs_register_args for col1change, col2change, edition_change, do_time, cb; funcs_register for pulseme, spawn_joker, rem_joker. G.C[args.to_val] uses a local lookup limited to the ten col_op names, since game.h has no generic G.C lookup. G.SANDBOX.col1/col2 are P(float *) and the draw-step send entries keep ref_table = T(G.SANDBOX) + ref_value, so Sprite_draw_shader must accept a P(colour) read through ref_table[ref_value]. The sandbox event returns false every pass (the Lua returns nil), so it never completes.

7. Game_save_metrics keeps the Lua quirk of setting FILE_HANDLER.settings rather than a metrics flag.

## game_6.c

game.lua:2449-3071 is translated as Game_update, Game_draw, Game_state_col and Game_update_selecting_hand. Statics: draw_node (the repeated push / translate_container / draw / pop block) and the two event closures ev_deck_preview_close (3024) and ev_deck_preview_cursor (3031).

Left as one-line comments, not translated:
- game.lua:2471, the 'LONG DT' print (debug print; it would fire on most PS2 frames).
- game.lua:2478-2481, the HTTP response pop.
- game.lua:2653-2666, the Steam stats flush.
- game.lua:2669-2683, the G.DEBUG object counters.
- game.lua:2971-3002, the FPS / perf overlay.

Notes:
- **Evaluation order:** the two DynaTexts of boss_warning_text, and the UIDEF_deck_preview() / create_UIBox_buttons() definitions, are built in their own statements before the UIBox_new(TAB(...)) call, so objects are created in the Lua's order.
- **Dyn fields:** UIBox attention_text is read and written in the box's dyn, and OVERLAY_TUTORIAL.highlights is read from its dyn, as ui.h and game.h say.
- **`v.draw_children` in the highlights loop** is is_a(v, &UIElement_class), then UIElement_draw_self / UIElement_draw_children by name.
- **`getmetatable(focused.target) == Card`** is an exact class test that accepts Card_class or Card_Character_card_class, because Jimbo's card keeps the Card metatable in the Lua.
- **remove_nils(G.ANIMATIONS)** is called and its result discarded, as the Lua does and misc_functions.h asks. game.h's comment "holes removed each frame" therefore only holds if remove_nils compacts in place.
- **Iteration:** pairs over G.I.*, MOVEABLES and ANIMATIONS is titer (array order). If move / update / draw removes entries during the loop, the result depends on titer's behaviour.
- **Save flush:** nil and false are told apart with veq for last_sent_pause against SETTINGS.paused; last_sent_stage is stored as N(G->STAGE) and last_sent_time as N(UPTIME); update_queued is not cleared, as in the Lua. save_profile is tgetv(G->PROFILES, SETTINGS.profile).
- **CRT pass:** translated literally with Shader_send / Shader_send_vec on SHADERS_get("CRT"), including the temporary crt*0.3 and /0.3 on the settings table. SHADERS_get("CRT") must be non-NULL or tolerated by love_gfx.
- **AA_CANVAS** is NULL, so love_graphics_setCanvas(NULL) draws the canvas to the screen and the second blit is skipped.
- **Dangling pointer risk:** ev_deck_preview_cursor keeps a plain Node * to CONTROLLER.focused.target for 0.4 s. The Lua upvalue keeps the object alive; in C the pointer dangles if the object is freed in that window (marked with a // note:).
- **OVERLAY_MENU_TRUE:** Game_draw would dereference G->OVERLAY_MENU if it still held that value during a draw. The Lua would fail there too.
- **Game_state_col** accepts NULL out-pointers.

## game_7.c

All 14 Game_update_* handlers of game.lua:3072-3629 are written.

Notes:
- Shared closures: the pack handlers' identical Lua closures are one static each (ev_pack_recall_focus, ev_pack_draw, ev_pack_nodraw), and ev_save_run serves both :3171 and :3265. Small static helpers cover the repeated lines: room_jiggle3, tags_apply, hand_box, remove_buttons/remove_shop, pack_every_frame, pack_sparkles, pack_box. Event order, delays and flags are as the Lua.
- Evaluation order: every UIBox definition (UIDEF_shop, create_UIBox_*) is built into a local before the config table, so G.ROOM.T.y is read after the definition runs, as in the Lua constructor. The lighten()/HEX() colours are likewise evaluated in Lua order before the Particles config.
- E-SH2 (:3089) and E-RE2 (:3322) return false where the Lua falls off the end, so they are retried. The -7.8 compare is float against -7.8f.
- shop_exists is captured in a ctx when update_shop runs and passed through E-SH1 to E-SH2. The joker_max bound is taken once.
- G.P_CARDS.empty is passed as ttab(G->P_CARDS, "empty"), which is NULL per game.h.
- In dyn: card.shop_voucher, G.CONTROLLER.lock_input, Jimbo.ui_object_updated, G.ROOM.jiggle.
- Game over event (:3610): if G->OVERLAY_MENU is OVERLAY_MENU_TRUE it raises ERROR, standing in for the Lua's error on indexing a boolean, instead of dereferencing (UIBox *)1.
- Literal Lua crashes kept unguarded: create_card_for_shop returning NULL goes straight to CardArea_emplace, and get_pack returning NULL stores nil in used_packs.
- math.random(1,10) for the quip is math_random_range(1, 10), the unseeded generator.
- -Werror=int-conversion passes only because nothing here passes a bare 0 as a pointer; all nil pointers are NULL/NIL.

## UI_definitions_10.c

Functions written (8 public): UIDEF_challenge_description_tab (:5680, split into three static helpers, one per tab), UIDEF_run_setup_option (:5961), create_button_binding_pip (:6126), create_UIBox_profile_button (:6158), create_UIBox_main_menu_buttons (:6194), create_UIBox_main_menu_competittion_name (:6245), UIDEF_language_selector (:6262), create_UIBox_highlight (:6302).

Notes:
- **`saved_game`** is defined here (`Table *saved_game;`), as UI_definitions.h says.
- **`create_UIBox_highlight`** is defined here as a strong symbol (the :6302 definition is the one in force). UI_definitions_1.c already defines the :463 copy as `__attribute__((weak))`, so the two link.
- **`UIDEF_blind_sprite_class`** is only used here (it is defined in UI_definitions_7.c). The banned-blind sprites get `class_init` if the class is not ready, then `cls` swapped, dyn `discovered = true` and `float = true`, and config replaced by `{blind, force_focus}`.
- **Construction order**: objects the Lua creates inside a table constructor (the `Moveable()` placeholders, the `UIBox{}` of the back description and the stake column, `create_option_cycle`, `create_toggle`, the `UIBox_button` calls, `create_text_input`) are created in locals first, in the Lua's order, because C does not fix argument evaluation order.
- **Small static helpers** (`node3`, `loc_text_row`, `run_setup_stat_row`, `moveable_new0`, `sprite_new`) build exactly the Lua's tables; they are not new behaviour.
- **G fields in dyn**: `G.run_setup_seed = nil` and `G.setup_seed = ''` are written on `G->object.dyn`; the seeded-run toggle uses `ref_table = O(G)`.
- **`G.GAME.viewed_back`** is stored as `O(Back *)` under the key `viewed_back` of `G->GAME`.
- **Lua errors kept as ERROR**: a challenge modifier whose id is not one of the seven base modifiers (:5701); the Continue tab with `saved_game` nil (:6044); `G.F_DISP_USERNAME` truthy but not a string (:6162).
- **Lua error not reproduced**: a nil challenge (bad `_id`) in the Rules and Restrictions tabs is read as "no rules / no restrictions", where the Lua would raise.
- **`G.OVERLAY_MENU`** is dereferenced when non-NULL (:5988, :5996). If it could be `OVERLAY_MENU_TRUE` at that moment the Lua would raise too, so no guard was added.
- **pairs order**: `base_modifiers`, `banned_tags`, `banned_other` and `G.LANGUAGES` are walked in insertion order and then sorted by a unique key, so the result matches. `G.P_CARDS` in the Deck tab is insertion order, then sorted per suit by nominal. `G.P_CENTERS` at :5978 is last-match-wins on the back's name.
- **`math.log(6, n)`** is `log(6)/log(n)` in doubles, with the same results at n = 0 and n = 1 as the Lua.
- **Tag size**: `Tag_generate_UI` takes a negative size as nil; `1.1 - 0.25*sqrt(n)` only goes negative with 20 or more banned tags, which no challenge has.
- **Float vs double**: `G->CARD_W` / `CARD_H` are floats in game.h, so card sizes are float products where the Lua has doubles (geometry only).
- **Deck seal key**: the code reads `_de.seal` as the Lua does; the comment in challenges.h says `gold_seal`.

## game_4.c

game.lua:1373-2017 is translated in full: Game_splash_screen, make_splash_card, Game_main_menu, Game_demo_cta and Game_init_game_object.

Notes:

- **os.time() (game.lua:1404):** love.h has no os_time, so the file includes <time.h> and calls time(NULL) for the splash vort_offset. If the PS2 build wants a platform wrapper, this is the one call site.
- **RESET_STATES (game.lua:1524):** left as a comment only, as io.h and `docs/header-notes.md` (io.h, point 2) require.
- **init_game_object was built in code, not loaded from game.dat, when the file was written.** Key order is the Lua's source order; keys the Lua sets to nil are absent. bosses_used follows titer order of G->P_BLINDS.
- **Card states mirroring:** where the Lua writes a card's states.hover / drag (the splash joker, the vortex cards), a static mirror_states() copies hover/click/drag to the card's front, center and back sprites, because the Lua shares those tables and node.h holds States by value.
- **G.SPLASH_LOGO.dissolve and dissolve_colours** are in the sprite's dyn, which is where sprite.c reads them for a plain Sprite; the logo ease is ease_value(logo dyn, "dissolve", ...).
- **Send lists:** entries that point into G use ref_float / ref_colour as game.h says; mid_flash in the menu keeps ref_table = T(splash_args), ref_value = "mid_flash".
- **G.CONTROLLER.lock_input = false** is written to the controller's dyn, per controller.h.
- **Vortex eases:** ease_value_f targets card T->scale / x / y, and the card is removed by the polling event once scale <= 0. The scale ease is the longest and is queued before that event, so no ease outlives the card (noted in the code).
- **Shared code:** main_menu and demo_cta have identical Lua blocks for the background sprite and for title_top / logo / replace_card; these are two static helpers (menu_splash_back, menu_title) with the same statement order. The 12 hands rows go through a static hand_row() that takes every Lua field explicitly.
- **Lua errors kept as ERROR():** main_menu aborts if SETTINGS.tutorial_progress.completed_parts is nil while tutorial_complete is falsy, and if the profile's career_stats is nil, as the Lua would.
- **Iteration order:** the career_stats loop runs in titer insertion order where the Lua uses hash order; only the order of the unlock checks changes.
- **Creation order in demo_cta:** the DynaText and the two UIBox_button tables are built in locals before the definition table, so objects are created in the Lua constructor's order.
- **Random call order:** kept explicit where it matters — in 'crumple'..math.random(1,5) the integer draw comes before the pitch draw, and in make_splash_card pseudorandom_element runs before the facing roll.

## main.c

Notes:
- **`Game *G` is defined in main.c.** `types.h` only declares it extern, and `game.h` gives `G = Game()` (globals.lua:522) to `main()`. globals.c and game_*.c must not define it too.
- **`main()` order:** `love_init(NULL)`, `classes_init()`, `G = NEW(Game)`, `Game_init(G)`, `math_randomseed(G->SEED)`, `return love_run()`.
- **Quit:** the platform has no "quit" event, so `love_run_frame` calls `love_frame_begin()` once per frame, right after `run_time = getTime()`. When it returns false, `love_quit()` runs and the frame returns true with exit code 0. This call sits outside the Lua's `G and G.CONTROLLER` test so frames are always counted.
- **Events:** only the three `LoveEvent` types are dispatched (gamepadpressed, gamepadreleased, joystickaxis). The Lua's touchpressed / deferred-mousepressed logic (main.lua:56-66) is a comment, since the pad never produces those events.
- **Frame timing:** `dt`, `dt_smooth` and `run_time` are file statics, doubles; `love_update` receives `(float)dt_smooth`. `G->FPS_CAP` of 0 (nil) becomes 500. `love_timer_sleep` is still called, though it is a no-op in `love.h`.
- **Present:** the main loop uses `love_present()`; `love_errhand` uses `love_graphics_present()` as the Lua does.
- **`love_errhand`:**
  - It returns at once with `F_NO_ERROR_HAND`.
  - Its loop exits when `love_frame_begin()` is false, which also counts frames there.
  - The escape-key and touch message-box exits are not reachable from the pad.
  - The printf wrap limit is dropped, because `love_graphics_printf` has no limit argument.
  - `debug.traceback()` becomes an empty string when `RELEASE_MODE` is false.
- **Not ported, left as comments:** Steam (main.lua:88-114, :123) and the HTTP crash report (:202-248).
- **`love_resize`:** the dead `'Windows' and false` block (:370-384) is translated and stays dead. `ROOM_ORIG` and `WINDOWTRANS` are assigned member by member.
- **`last_touch_time`** lives in `G->CONTROLLER->object.dyn`, as `controller.h` says.

Depends on:
- void classes_init(void); in include/needs/main.h -- node.h:28 mentions it but no shared header declares it (same prototype as include/needs/object.h; src/object.c exports it). main() calls it once before G = NEW(Game).

## globals.c

Game_set_globals(Game *self) is written against game.h (there is no globals.h).

- Data source: it calls data_load("data/game.dat") itself and uses section "globals". The tables SETTINGS, COLLABS, METRICS, PROFILES, STAGE_OBJECTS, ARGS, I.*, ANIMATION_ATLAS, ASSET_ATLAS, MOVEABLES, ANIMATIONS, DRAW_HASH, handlist and keybind_mapping become G's directly, not copies. A missing table is an ERROR, not a silent NULL.
- Memory: the rest of game.dat stays allocated after the call. game_2.c loads its own second copy of the same file. A shared loader is the fix; both files carry a `ponytail:` comment saying so.
- Flags: the data file holds the flags after the OS blocks for the dump's OS ('ps4'), not the Lua defaults of :13-42. The C loads those and then runs the five OS blocks (:45-120) as code, which reassigns the same values. If love_system_getOS() ever answers something other than the dump's DUMP_OS, the result is wrong and the dump must be redone.
- F_RUMBLE and SETTINGS.rumble: F_RUMBLE uses 0 as nil. SETTINGS.rumble is set from it in code (:160), N(0.5) on ps4, and the key is removed when F_RUMBLE is 0.
- SEED: `(num)time(NULL)` from libc, the same choice game_4.c made, since love.h has no os.time.
- HIGHLIGHT_H: computed in code as 0.2 * CARD_H in doubles, then cast to float.
- Enums and registry: STATES, STAGES, UIT and FUNCS have nothing to assign. STAGE is STAGES_MAIN_MENU, STATE is STATES_SPLASH, TAROT_INTERRUPT is 0.
- Colours: the four numbers of each are copied from the data into G->C.
  - The SUITS literals go to C.SUITS_init, and the C.SUITS.* pointers point at them.
  - HAND_LEVELS[0] is a memcpy of RED (a copy, as game.h specifies), and UI_CHIPS / UI_MULT are memcpys of BLUE / RED.
  - STAKES, UI_CHIPLICK and UI_MULTLICK are left zero, and exp_times.max_vel is not set (the Lua does not set it here).
- button_mapping: built in code from the two swap flags; it is an empty table on ps4.
- Lines 1-2 and 522: VERSION comes from the data ('1.0.1o-FULL'); `G = Game()` belongs to main.c per game.h.

## tag.c

All 14 functions of tag.h are written.

Notes:
- tag.c defines both class objects: Tag_class (super Object_class, slots juice_up and remove) and Tag_sprite_class (name "Sprite", super Sprite_class, slots hover and stop_hover). Tag_generate_UI calls class_init(&Tag_sprite_class) if it is not ready, then swaps the sprite's cls.
- Controller locks use tseti(G->CONTROLLER->locks, ID, ...) as tag.h says. controller.h's comment says tsetv(locks, N(ID)); these must hit the same key in core.c or a tag's lock will never read as set or released.
- The five booster-tag closures (Charm, Meteor, Ethereal, Standard, Buffoon) are one function, ev_pack, differing only in the key. math.random(1,2) is still drawn inside the event, as in the Lua.
- The four edition-tag closures are one function, ev_edition. It keeps the Lua's order difference: Foil clears temp_edition after set_cost, the other three before set_edition.
- These card fields are in the Card's dyn, because card.h declares no such members: from_tag, temp_edition.
- G.CONTROLLER.using_touch is read from the Controller's dyn, as controller.h says.
- G.P_CARDS.empty is passed as ttab(G->P_CARDS, "empty"), which is NULL (nil in the Lua too).
- Where the Lua would raise an error, the C calls ERROR instead of dereferencing NULL: self.HUD_tag nil in the nope/yep hide events and in Tag_remove, G.HUD_tags nil in Tag_remove, and G.GAME.hands[orbital_hand] nil for an Orbital Tag still holding the placeholder.
- One deviation: Tag_load treats a nil G.GAME.tag_tally as 0, where the Lua's math.max would error.
- Tag_remove_from_game keeps the Lua quirk: if the tag is not in G.GAME.tags, the last tag is removed (tremove with pos 0).
- Tag_remove uses Moveable_set_alignment and Moveable_set_role on the next HUD box, since ui.h has no UIBox override of either.
- In 'store_joker_create', triggered = true and the card return happen for any tag of that type, as the Lua does despite its indentation. A Rare Tag with no rare left returns NIL.

## Not ported

The places where the C stops with `TODO_PORT` instead of doing what the Lua does. They are the demo video, the debug tools and the online high scores.

- `sprite.c`: sprite.lua:134 self.video draw path in Sprite_draw_self (love.h has no videos); aborts only if dyn 'video' is set (only UI_definitions.lua:2853, the demo video CTA, sets it)
- `sprite.c`: sprite.lua:202 self.video:release() in Sprite_remove; same condition
- `UI_definitions_1.c`: UI_definitions.lua:5 create_UIBox_debug_tools: debug only (and its 16 G.FUNCS.DT_*), body is TODO_PORT as UI_definitions.h prescribes
- `UI_definitions_1.c`: UI_definitions.lua:196 create_UIBox_online_high_scores: HTTP manager, body is TODO_PORT as UI_definitions.h prescribes
- `UI_definitions_1.c`: UI_definitions.lua:206 create_UIBox_high_scores_filling: HTTP manager (loadstring of the response), body is TODO_PORT as UI_definitions.h prescribes
- `UI_definitions_5.c`: UI_definitions.lua:2850 create_UIBox_demo_video_CTA: love.graphics.newVideo and everything after it (the video sprite, its volume, play(), the generic options box around it) - love.h has no video; demo only. The event at :2836 and RESTART_MUSIC() at :2848 are translated before it.
