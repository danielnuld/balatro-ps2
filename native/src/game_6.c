// game.lua:2449-3071: Game:update, Game:draw, Game:state_col, Game:update_selecting_hand
#include "all.h"

// love.graphics.push(); v:translate_container(); v:draw(); love.graphics.pop() (the block Game:draw repeats)
static void draw_node(void *v) {
    love_graphics_push();
    Node_translate_container((Node *)v);
    CALL(v, draw);
    love_graphics_pop();
}

// game.lua:2449
void Game_update(Game *self, num dt) {
    nuGC(0, 0, true);

    G->MAJORS = 0;
    G->MINORS = 0;

    G->FRAMES.MOVE = G->FRAMES.MOVE + 1;
    timer_checkpoint("start->discovery", "update", false);
    if (!tbool(G->SETTINGS, "tutorial_complete")) FUNCS_tutorial_controller();
    timer_checkpoint("tallies", "update", false);
    modulate_sound(dt);
    timer_checkpoint("sounds", "update", false);
    update_canvas_juice(dt);
    timer_checkpoint("canvas and juice", "update", false);
    // Smooth out the dts to avoid any big jumps
    self->TIMERS.REAL = self->TIMERS.REAL + dt;
    self->TIMERS.REAL_SHADER = SETTINGS_snap()->reduced_motion ? 300 : self->TIMERS.REAL;
    self->TIMERS.UPTIME = self->TIMERS.UPTIME + dt;
    {
        Table *DEMO = ttab(self->SETTINGS, "DEMO");
        Val total_uptime = tget(DEMO, "total_uptime");
        tsetn(DEMO, "total_uptime", (truthy(total_uptime) ? total_uptime.n : 0) + dt);
    }
    {
        Table *spin = ttab(G->ARGS, "spin");
        Val amount = tget(spin, "amount"); // G.ARGS.spin and G.ARGS.spin.amount or 0
        self->TIMERS.BACKGROUND = self->TIMERS.BACKGROUND + dt * (truthy(amount) ? amount.n : 0);
    }
    self->real_dt = dt;

    // game.lua:2471 print('LONG DT @ ...') when real_dt > 0.05: debug print, not translated
    if (!G->fbf || G->new_frame) {
        G->new_frame = false;

        set_alerts();
        timer_checkpoint("alerts", "update", false);

        // game.lua:2478-2481 G.HTTP_MANAGER.in_channel:pop(): the HTTP manager is not translated (always nil)

        if (SETTINGS_snap()->paused) dt = 0;

        if (G->STATE != G->ACC_state) G->ACC = 0;
        G->ACC_state = G->STATE;

        if (G->STATE == STATES_HAND_PLAYED || G->STATE == STATES_NEW_ROUND) {
            G->ACC = lmin(G->ACC + dt * 0.2 * tnum(self->SETTINGS, "GAMESPEED"), 16);
        } else {
            G->ACC = 0;
        }

        self->SPEEDFACTOR = (G->STAGE == STAGES_RUN && !SETTINGS_snap()->paused && !G->screenwipe) ? tnum(self->SETTINGS, "GAMESPEED") : 1;
        self->SPEEDFACTOR = self->SPEEDFACTOR + lmax(0, fabs(G->ACC) - 2);

        self->TIMERS.TOTAL = self->TIMERS.TOTAL + dt * (self->SPEEDFACTOR);

        // (colours: float, docs/conventions.md; the PS2 has no double FPU and its sin() is all library calls)
        float REALf = (float)self->TIMERS.REAL;
        self->C.DARK_EDITION[0] = 0.6f + 0.2f * sinf(wrap_angle(REALf * 1.3f));
        self->C.DARK_EDITION[2] = 0.6f + 0.2f * (1 - sinf(wrap_angle(REALf * 1.3f)));
        self->C.DARK_EDITION[1] = fminf(self->C.DARK_EDITION[2], self->C.DARK_EDITION[0]);

        self->C.EDITION[0] = 0.7f + 0.2f * (1 + sinf(wrap_angle(REALf * 1.5f + 0)));
        self->C.EDITION[2] = 0.7f + 0.2f * (1 + sinf(wrap_angle(REALf * 1.5f + 3)));
        self->C.EDITION[1] = 0.7f + 0.2f * (1 + sinf(wrap_angle(REALf * 1.5f + 6)));

        EventManager_update(self->E_MANAGER, self->real_dt, false);
        timer_checkpoint("e_manager", "update", false);

        Blind *blind = (Blind *)tobj(G->GAME, "blind");
        if (blind && G->boss_throw_hand && self->STATE == STATES_SELECTING_HAND) {
            if (!self->boss_warning_text) {
                // note: the two DynaTexts are made in statements of their own, in the Lua's order (C does not
                // order the evaluation of arguments)
                DynaText *text1 = DynaText_new(TAB("scale", N(0.7), "string", localize(S("ph_unscored_hand"), NULL), "maxw", N(9),
                                                   "colours", T(ARR(P(G->C.WHITE))), "float", B(true), "shadow", B(true),
                                                   "silent", B(true), "pop_in", N(0), "pop_in_rate", N(6)));
                DynaText *text2 = DynaText_new(TAB("scale", N(0.6), "string", S(Blind_get_loc_debuff_text(blind)), "maxw", N(9),
                                                   "colours", T(ARR(P(G->C.WHITE))), "float", B(true), "shadow", B(true),
                                                   "silent", B(true), "pop_in", N(0), "pop_in_rate", N(6)));
                Table *definition =
                    TAB("n", N(UIT_ROOT), "config", T(TAB("align", S("cm"), "colour", P(G->C.CLEAR), "padding", N(0.2))), "nodes", T(ARR(
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(1))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(text1)))))
                        )))),
                        T(TAB("n", N(UIT_R), "config", T(TAB("align", S("cm"), "maxw", N(1))), "nodes", T(ARR(
                            T(TAB("n", N(UIT_O), "config", T(TAB("object", O(text2)))))
                        ))))
                    )));
                self->boss_warning_text = UIBox_new(TAB(
                    "definition", T(definition),
                    "config", T(TAB(
                        "align", S("cm"),
                        "offset", T(TAB("x", N(0), "y", N(-3.1))),
                        "major", O(G->play)))));
                self->boss_warning_text->attention_text = true;
                self->boss_warning_text->moveable.node.states.collide.can = false;
                CALL(Blind_sprite(blind), juice_up, 0.05f, N(0.02));
                play_sound("chips1", math_random() * 0.1 + 0.55, 0.12f);
            }
        } else {
            G->boss_throw_hand = false;
            if (self->boss_warning_text) {
                CALL(self->boss_warning_text, remove);
                self->boss_warning_text = NULL;
            }
        }

        if (self->STATE == STATES_SELECTING_HAND) {
            if (!CardArea_card(G->hand, 1) && CardArea_card(G->deck, 1)) {
                G->STATE = STATES_DRAW_TO_HAND;
                G->STATE_COMPLETE = false;
            } else {
                Game_update_selecting_hand(self, dt);
            }
        }

        if (self->STATE == STATES_SHOP) {
            Game_update_shop(self, dt);
        }

        if (self->STATE == STATES_PLAY_TAROT) {
            Game_update_play_tarot(self, dt);
        }

        if (self->STATE == STATES_HAND_PLAYED) {
            Game_update_hand_played(self, dt);
        }

        if (self->STATE == STATES_DRAW_TO_HAND) {
            Game_update_draw_to_hand(self, dt);
        }

        if (self->STATE == STATES_NEW_ROUND) {
            Game_update_new_round(self, dt);
        }

        if (self->STATE == STATES_BLIND_SELECT) {
            Game_update_blind_select(self, dt);
        }

        if (self->STATE == STATES_ROUND_EVAL) {
            Game_update_round_eval(self, dt);
        }

        if (self->STATE == STATES_TAROT_PACK) {
            Game_update_arcana_pack(self, dt);
        }

        if (self->STATE == STATES_SPECTRAL_PACK) {
            Game_update_spectral_pack(self, dt);
        }

        if (self->STATE == STATES_STANDARD_PACK) {
            Game_update_standard_pack(self, dt);
        }

        if (self->STATE == STATES_BUFFOON_PACK) {
            Game_update_buffoon_pack(self, dt);
        }

        if (self->STATE == STATES_PLANET_PACK) {
            Game_update_celestial_pack(self, dt);
        }

        if (self->STATE == STATES_GAME_OVER) {
            Game_update_game_over(self, dt);
        }

        if (self->STATE == STATES_MENU) {
            Game_update_menu(self, dt);
        }
        timer_checkpoint("states", "update", false);
        // animate all animated objects
        remove_nils(self->ANIMATIONS); // the Lua discards the result too

        for (TArr it = tarr(self->ANIMATIONS); tarr_next(&it);) {
            void *v = it.val.obj;
            CALL(v, animate, (float)(self->real_dt * self->SPEEDFACTOR));
        }
        timer_checkpoint("animate", "update", false);

        // move and update all other moveables
        G->exp_times.xy = exp(-50 * self->real_dt);
        G->exp_times.scale = exp(-60 * self->real_dt);
        G->exp_times.r = exp(-190 * self->real_dt);

        float move_dt = (float)lmin(1.0 / 20, self->real_dt);

        G->exp_times.max_vel = 70 * move_dt;

        // note: pairs(self.MOVEABLES) is titer: the array's order, as LuaJIT walks an array
        for (TArr it = tarr(self->MOVEABLES); tarr_next(&it);) {
            Node *v = (Node *)it.val.obj;
            if (v->FRAME.MOVE < G->FRAMES.MOVE) CALL(v, move, move_dt);
        }
        timer_checkpoint("move", "update", false);

        float update_dt = (float)(dt * self->SPEEDFACTOR); // (once: only this function assigns SPEEDFACTOR)
        for (TArr it = tarr(self->MOVEABLES); tarr_next(&it);) {
            Node *v = (Node *)it.val.obj;
            CALL(v, update, update_dt);
            v->states.collide.is = false;
        }
        timer_checkpoint("update", "update", false);
    }

    Controller_update(self->CONTROLLER, (float)self->real_dt);

    // update loc strings if needed
    Table *round_resets = ttab(G->GAME, "round_resets");
    Table *blind_states = ttab(round_resets, "blind_states");
    if (!streq(G->prev_small_state, tstr(blind_states, "Small")) ||
        !streq(G->prev_large_state, tstr(blind_states, "Big")) ||
        !streq(G->prev_boss_state, tstr(blind_states, "Boss")) || G->RESET_BLIND_STATES) {
        G->RESET_BLIND_STATES = false;
        G->prev_small_state = tstr(blind_states, "Small");
        G->prev_large_state = tstr(blind_states, "Big");
        G->prev_boss_state = tstr(blind_states, "Boss");
        Table *loc_blind_states = ttab(round_resets, "loc_blind_states");
        tset(loc_blind_states, "Small", localize(tget(blind_states, "Small"), "blind_states"));
        tset(loc_blind_states, "Big", localize(tget(blind_states, "Big"), "blind_states"));
        tset(loc_blind_states, "Boss", localize(tget(blind_states, "Boss"), "blind_states"));
    }

    // game.lua:2653-2666 Send all steam updates if needed: Steam is not translated

    // game.lua:2669-2683 G.DEBUG object counters: debug code is not translated

    // Save every 10 seconds, unless forced or paused/unpaused
    Table *FH = G->FILE_HANDLER;
    if (FH && tbool(FH, "update_queued") && (
        tbool(FH, "force") ||
        !veq(tget(FH, "last_sent_stage"), N(G->STAGE)) ||
        (!veq(tget(FH, "last_sent_pause"), tget(G->SETTINGS, "paused")) && tbool(FH, "run")) ||
        (!tbool(FH, "last_sent_time") || (tnum(FH, "last_sent_time") < (G->TIMERS.UPTIME - G->F_SAVE_TIMER))))) {

        if (tbool(FH, "metrics")) {
            save_manager_request(TAB(
                "type", S("save_metrics"),
                "save_metrics", tget(G->ARGS, "save_metrics")));
        }

        if (tbool(FH, "progress")) {
            save_manager_request(TAB(
                "type", S("save_progress"),
                "save_progress", tget(G->ARGS, "save_progress")));
        } else if (tbool(FH, "settings")) {
            save_manager_request(TAB(
                "type", S("save_settings"),
                "save_settings", tget(G->ARGS, "save_settings"),
                "profile_num", tget(G->SETTINGS, "profile"),
                "save_profile", tgetv(G->PROFILES, tget(G->SETTINGS, "profile"))));
        }

        if (tbool(FH, "run")) {
            save_manager_request(TAB(
                "type", S("save_run"),
                "save_table", tget(G->ARGS, "save_run"),
                "profile_num", tget(G->SETTINGS, "profile")));
            G->SAVED_GAME = NULL;
        }

        tsetb(FH, "force", false);
        tsetn(FH, "last_sent_stage", G->STAGE);
        tsetn(FH, "last_sent_time", G->TIMERS.UPTIME);
        tset(FH, "last_sent_pause", tget(G->SETTINGS, "paused"));
        tset(FH, "settings", NIL);
        tset(FH, "progress", NIL);
        tset(FH, "metrics", NIL);
        tset(FH, "run", NIL);
    }
}

// game.lua:2732
void Game_draw(Game *self) {
    G->FRAMES.DRAW = G->FRAMES.DRAW + 1;
    // draw the room
    reset_drawhash();
    if (G->OVERLAY_TUTORIAL && !G->OVERLAY_MENU) G->under_overlay = true;
    timer_checkpoint("start->canvas", "draw", false);
    love_graphics_setCanvas(self->CANVAS);
    love_graphics_push();
    love_graphics_scale(G->CANV_SCALE, G->CANV_SCALE);

    love_graphics_setShader(NULL);
    love_graphics_clear(0, 0, 0, 1);

    if (G->SPLASH_BACK) {
        if (G->debug_background_toggle) {
            love_graphics_clear(0, 1, 0, 1);
        } else {
            draw_node(G->SPLASH_BACK);
        }
    }

    if (!G->debug_UI_toggle) {

        for (TArr it = tarr(self->I.NODE); tarr_next(&it);) {
            Node *v = (Node *)it.val.obj;
            if (!v->parent) draw_node(v);
        }

        for (TArr it = tarr(self->I.MOVEABLE); tarr_next(&it);) {
            Node *v = (Node *)it.val.obj;
            if (!v->parent) draw_node(v);
        }

        if (G->SPLASH_LOGO) {
            draw_node(G->SPLASH_LOGO);
        }

        if (G->debug_splash_size_toggle) {
            for (TArr it = tarr(self->I.CARDAREA); tarr_next(&it);) {
                Node *v = (Node *)it.val.obj;
                if (!v->parent) draw_node(v);
            }
        } else {
            if (!self->OVERLAY_MENU || !self->F_HIDE_BG) {
                timer_checkpoint("primatives", "draw", false);
                for (TArr it = tarr(self->I.UIBOX); tarr_next(&it);) {
                    Node *v = (Node *)it.val.obj;
                    if (!((UIBox *)v)->attention_text && !v->parent && v != (Node *)self->OVERLAY_MENU &&
                        v != (Node *)self->screenwipe && v != (Node *)self->OVERLAY_TUTORIAL && v != (Node *)self->debug_tools &&
                        v != (Node *)self->online_leaderboard && v != (Node *)self->achievement_notification) {
                        draw_node(v);
                    }
                }
                timer_checkpoint("uiboxes", "draw", false);
                for (TArr it = tarr(self->I.CARDAREA); tarr_next(&it);) {
                    Node *v = (Node *)it.val.obj;
                    if (!v->parent) draw_node(v);
                }

                for (TArr it = tarr(self->I.CARD); tarr_next(&it);) {
                    Node *v = (Node *)it.val.obj;
                    if (!v->parent && v != self->CONTROLLER->dragging.target && v != self->CONTROLLER->focused.target) {
                        draw_node(v);
                    }
                }

                for (TArr it = tarr(self->I.UIBOX); tarr_next(&it);) {
                    Node *v = (Node *)it.val.obj;
                    if (((UIBox *)v)->attention_text && v != (Node *)self->debug_tools &&
                        v != (Node *)self->online_leaderboard && v != (Node *)self->achievement_notification) {
                        draw_node(v);
                    }
                }

                if (G->SPLASH_FRONT) {
                    draw_node(G->SPLASH_FRONT);
                }

                G->under_overlay = false;
                if (self->OVERLAY_TUTORIAL) {
                    draw_node(self->OVERLAY_TUTORIAL);

                    Table *highlights = ttab(self->OVERLAY_TUTORIAL->moveable.node.object.dyn, "highlights");
                    if (highlights) {
                        for (int k = 1; truthy(tgeti(highlights, k)); k++) {
                            void *v = tgeti(highlights, k).obj;
                            love_graphics_push();
                            Node_translate_container((Node *)v);
                            CALL(v, draw);
                            if (is_a(v, &UIElement_class)) { // v.draw_children
                                UIElement_draw_self((UIElement *)v);
                                UIElement_draw_children((UIElement *)v);
                            }
                            love_graphics_pop();
                        }
                    }
                }
            }
            if (self->OVERLAY_MENU || !self->F_HIDE_BG) {
                if (self->OVERLAY_MENU && (Node *)self->OVERLAY_MENU != self->CONTROLLER->dragging.target) {
                    draw_node(self->OVERLAY_MENU);
                }
            }

            if (self->debug_tools) {
                if ((Node *)self->debug_tools != self->CONTROLLER->dragging.target) {
                    draw_node(self->debug_tools);
                }
            }

            G->ALERT_ON_SCREEN = false;
            for (TArr it = tarr(self->I.ALERT); tarr_next(&it);) {
                void *v = it.val.obj;
                love_graphics_push();
                Node_translate_container((Node *)v);
                CALL(v, draw);
                G->ALERT_ON_SCREEN = true;
                love_graphics_pop();
            }

            if (self->CONTROLLER->dragging.target && self->CONTROLLER->dragging.target != self->CONTROLLER->focused.target) {
                draw_node(G->CONTROLLER->dragging.target);
            }

            // note: getmetatable(target) == Card is the exact class; Jimbo's card keeps the metatable Card in the
            // Lua (only its instance's hover is replaced), so its class of card_character.h counts too
            Node *focused = self->CONTROLLER->focused.target;
            if (focused && (focused->object.cls == &Card_class || focused->object.cls == &Card_Character_card_class) &&
                (((Card *)focused)->area != G->hand || focused == self->CONTROLLER->dragging.target)) {
                draw_node(G->CONTROLLER->focused.target);
            }

            for (TArr it = tarr(self->I.POPUP); tarr_next(&it);) {
                draw_node(it.val.obj);
            }

            if (self->achievement_notification) {
                draw_node(self->achievement_notification);
            }

            if (self->screenwipe) {
                draw_node(self->screenwipe);
            }

            love_graphics_push();
            Node_translate_container((Node *)self->CURSOR);
            love_graphics_translate(-self->CURSOR->moveable.node.T->w * G->TILESCALE * G->TILESIZE * 0.5f,
                                    -self->CURSOR->moveable.node.T->h * G->TILESCALE * G->TILESIZE * 0.5f);
            CALL(self->CURSOR, draw);
            love_graphics_pop();
            timer_checkpoint("rest", "draw", false);
        }
    }
    love_graphics_pop();

    love_graphics_setCanvas(G->AA_CANVAS);
    love_graphics_push();
    love_graphics_setColorv(G->C.WHITE);
    if ((!G->recording_mode || G->video_control) && true) {
        if (!truthy(tget(G->ARGS, "eased_cursor_pos"))) {
            tsett(G->ARGS, "eased_cursor_pos", TAB("x", N(G->CURSOR->moveable.node.T->x), "y", N(G->CURSOR->moveable.node.T->y),
                                                   "sx", N(G->CONTROLLER->cursor_position.x), "sy", N(G->CONTROLLER->cursor_position.y)));
        }
        Table *eased_cursor_pos = ttab(G->ARGS, "eased_cursor_pos");
        G->screenwipe_amt = G->screenwipe_amt_set ? (0.95 * G->screenwipe_amt + 0.05 * ((self->screenwipe ? 0.4 : self->screenglitch ? 0.4 : 0))) : 1;
        G->screenwipe_amt_set = true;
        Table *GRAPHICS = ttab(G->SETTINGS, "GRAPHICS");
        tsetn(GRAPHICS, "crt", tnum(GRAPHICS, "crt") * 0.3);
        num crt = tnum(GRAPHICS, "crt");
        Shader *CRT = SHADERS_get("CRT");
        Shader_send_vec(CRT, "distortion_fac", (float[2]){1.0 + 0.07 * crt / 100, 1.0 + 0.1 * crt / 100}, 2);
        Shader_send_vec(CRT, "scale_fac", (float[2]){1.0 - 0.008 * crt / 100, 1.0 - 0.008 * crt / 100}, 2);
        Shader_send(CRT, "feather_fac", 0.01f);
        Shader_send(CRT, "bloom_fac", tnum(GRAPHICS, "bloom") - 1);
        Shader_send(CRT, "time", 400 + G->TIMERS.REAL);
        Shader_send(CRT, "noise_fac", 0.001 * crt / 100);
        Shader_send(CRT, "crt_intensity", 0.16 * crt / 100);
        Shader_send(CRT, "glitch_intensity", 0); // 0.1*G.SETTINGS.GRAPHICS.crt/100 + (G.screenwipe_amt) + 1)
        Shader_send(CRT, "scanlines", Image_getPixelHeight(G->CANVAS) * 0.75f / G->CANV_SCALE);
        if (G->video_control) {
            Shader_send_vec(CRT, "mouse_screen_pos", (float[2]){love_graphics_getWidth() / 2.0f, love_graphics_getHeight() / 2.0f}, 2);
        } else {
            Shader_send_vec(CRT, "mouse_screen_pos", (float[2]){tnum(eased_cursor_pos, "sx"), tnum(eased_cursor_pos, "sy")}, 2);
        }
        Shader_send(CRT, "screen_scale", G->TILESCALE * G->TILESIZE);
        Shader_send(CRT, "hovering", 1);
        love_graphics_setShader(CRT);
        tsetn(GRAPHICS, "crt", tnum(GRAPHICS, "crt") / 0.3);
    }

    love_graphics_draw(self->CANVAS, 0, 0, 0, 1, 1, 0, 0);
    love_graphics_pop();

    love_graphics_setCanvas(NULL);
    love_graphics_setShader(NULL);

    if (G->AA_CANVAS) {
        love_graphics_push();
        love_graphics_scale(1 / G->CANV_SCALE, 1 / G->CANV_SCALE);
        love_graphics_draw(G->AA_CANVAS, 0, 0, 0, 1, 1, 0, 0);
        love_graphics_pop();
    }

    timer_checkpoint("canvas", "draw", false);

    // game.lua:2971-3002 the FPS and perf overlay (not _RELEASE_MODE and G.DEBUG): debug code is not translated
    timer_checkpoint("debug", "draw", false);
}

// game.lua:3006
float Game_state_col(Game *self, num _state, float *g, float *b, float *a) {
    if (g) *g = lmod(_state * 1422.5641311 / 5.42, 1);
    if (b) *b = lmod(_state * 1522.1523122 / 5.132, 1);
    if (a) *a = 1;
    return lmod(_state * 15251252.2 / 5.132, 1);
}

// game.lua:3030 the closure's upvalues _card and start
struct ev_deck_preview_cursor_ctx {
    Node *_card;
    num start;
};

// game.lua:3031
static bool ev_deck_preview_cursor(void *ctx) {
    struct ev_deck_preview_cursor_ctx *c = ctx;
    Node *_card = c->_card;
    // note: _card.area exists only on a Card (nil on any other node). The Lua keeps _card alive through the
    // upvalue; here it is a plain pointer, read for 0.4 s after the preview closes.
    if (_card && is_a(_card, &Card_class) && ((Card *)_card)->area && ((Card *)_card)->area == G->hand) {
        float _x = 0, _y = 0;
        CALL(_card, put_focused_cursor, &_x, &_y);
        Vec2 pos = {_x / (G->TILESCALE * G->TILESIZE), _y / (G->TILESCALE * G->TILESIZE)};
        Controller_update_cursor(G->CONTROLLER, &pos);
    }
    if (c->start + 0.4 < G->TIMERS.REAL) {
        return true;
    }
    return false;
}

// game.lua:3024
static bool ev_deck_preview_close(void *ctx) {
    Game *self = G;
    Card *deck1 = self->deck ? CardArea_card(self->deck, 1) : NULL;
    if (self->deck_preview && !((deck1 && deck1->moveable.node.states.collide.is && !self->CONTROLLER->HID.controller) ||
                                tbool(G->CONTROLLER->held_buttons, "triggerleft"))) {
        CALL(self->deck_preview, remove);
        self->deck_preview = NULL;
        struct ev_deck_preview_cursor_ctx *c = NEWCTX(struct ev_deck_preview_cursor_ctx);
        c->_card = G->CONTROLLER->focused.target;
        c->start = G->TIMERS.REAL;
        E_add(self->E_MANAGER, Event_new((EventCfg){.func = ev_deck_preview_cursor, .ctx = c}), NULL, false);
        return true;
    }
    return false;
}

// game.lua:3010
void Game_update_selecting_hand(Game *self, float dt) {
    Card *deck1 = self->deck ? CardArea_card(self->deck, 1) : NULL;
    if (!self->deck_preview && !G->OVERLAY_MENU && (
        (deck1 && deck1->moveable.node.states.collide.is && (!deck1->moveable.node.states.drag.is || self->CONTROLLER->HID.touch) && !self->CONTROLLER->HID.controller) ||
        tbool(G->CONTROLLER->held_buttons, "triggerleft"))) {
        if (self->buttons) {
            self->buttons->moveable.node.states.visible = false;
        }
        Table *definition = UIDEF_deck_preview();
        self->deck_preview = UIBox_new(TAB(
            "definition", T(definition),
            "config", T(TAB("align", S("tm"), "offset", T(TAB("x", N(0), "y", N(-0.8))), "major", O(self->hand), "bond", S("Weak")))));
        E_add(self->E_MANAGER, Event_new((EventCfg){
            .not_blocking = true,
            .not_blockable = true,
            .func = ev_deck_preview_close}), NULL, false);
    }
    if (!self->buttons && !self->deck_preview) {
        Table *definition = create_UIBox_buttons();
        self->buttons = UIBox_new(TAB(
            "definition", T(definition),
            "config", T(TAB("align", S("bm"), "offset", T(TAB("x", N(0), "y", N(0.3))), "major", O(G->hand), "bond", S("Weak")))));
    }
    if (self->buttons && !self->buttons->moveable.node.states.visible && !self->deck_preview) {
        self->buttons->moveable.node.states.visible = true;
    }

    if (CardArea_n(G->hand) < 1 && CardArea_n(G->deck) < 1 && CardArea_n(G->play) < 1) {
        end_round();
    }

    if (self->shop) {
        CALL(self->shop, remove);
        self->shop = NULL;
    }
    if (!G->STATE_COMPLETE) {
        G->STATE_COMPLETE = true;
        if (CardArea_n(G->hand) < 1 && CardArea_n(G->deck) < 1) {
            end_round();
        } else {
            save_run();
            Controller_recall_cardarea_focus(G->CONTROLLER, S("hand"));
        }
    }
}
