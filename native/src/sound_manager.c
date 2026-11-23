// engine/sound_manager.lua:1-207. The Lua is a thread fed through a channel; here a request runs inline (io.h).
#include "all.h"

// sound_manager.lua:1-5: the requires and the jit.off for arm Macs have no equivalent.
// sound_manager.lua:8-10: CHANNEL and LOAD_CHANNEL do not exist (io.h); the load_channel pushes are never read.
bool DISABLE_SFX = false; // sound_manager.lua:11
Table *SOURCES;           // sound_manager.lua:14

// note: a number read for arithmetic; nil raises as Lua's "attempt to perform arithmetic on a nil value".
static num arith_val(Val v, const char *key) {
    if (v.t != V_NUM && v.t != V_STR) ERROR("attempt to perform arithmetic on field '%s' (a %s value)", key, lua_typename_of(v));
    return vnum(v);
}
#define arith(t, key) arith_val(tget(t, key), key) // (a macro: the literal key is interned once per call site, core.h)

// note: s.sound for a method call; nil raises as Lua's index error.
static Source *snd(Table *s) {
    Val v = tget(s, "sound");
    if (!truthy(v)) ERROR("attempt to index field 'sound' (a nil value)");
    return (Source *)v.p;
}

// sound_manager.lua:13-35 and :183: create all sounds from resources and play one each to load into mem
void sound_manager_init(void) {
    SOURCES = tnew(); // :14
    int n = 0;
    const char **sound_files = love_filesystem_getDirectoryItems("resources/sounds", &n); // :15

    for (int f = 0; f < n; f++) { // :17
        const char *filename = sound_files[f];
        const char *extension = str_sub(filename, -4, -1); // :18
        for (int i = 1; i <= 1; i++) {                      // :19
            if (streq(extension, ".ogg")) {                 // :20
                // :21 LOAD_CHANNEL:push('audio file - '..filename)
                const char *sound_code = str_sub(filename, 1, -5); // :22
                const char *filepath = fmt("resources/sounds/%s", filename);
                Table *s = TAB( // :23
                    "sound", P(love_audio_newSource(filepath, str_find(sound_code, "music", 1) ? "stream" : "static")),
                    "filepath", S(filepath));
                tsett(SOURCES, sound_code, tnew());             // :27
                tinsert(ttab(SOURCES, sound_code), T(s));       // :28
                tsets(s, "sound_code", sound_code);             // :29
                Source_setVolume(snd(s), 0);                    // :30
                love_audio_play(snd(s));                        // :31
                Source_stop(snd(s));                            // :32
            }
        }
    }
    free(sound_files);
    // :183 LOAD_CHANNEL:push('finished')
}

// sound_manager.lua:37
Table *PLAY_SOUND(Table *args) {
    if (!truthy(tget(args, "per"))) tsetn(args, "per", 1); // :38
    if (!truthy(tget(args, "vol"))) tsetn(args, "vol", 1); // :39
    const char *sound_code = tstr(args, "sound_code");
    if (!sound_code) ERROR("PLAY_SOUND: args.sound_code is nil"); // note: SOURCES[nil] = ... is an error in Lua
    if (!truthy(tget(SOURCES, sound_code))) tsett(SOURCES, sound_code, tnew()); // :40

    Table *list = ttab(SOURCES, sound_code);
    for (int i = 1; truthy(tgeti(list, i)); i++) { // :42
        Table *s = ttabi(list, i);
        if (truthy(tget(s, "sound")) && !Source_isPlaying(snd(s))) { // :43
            tset(s, "original_pitch", tget(args, "per"));            // :44
            tset(s, "original_volume", tget(args, "vol"));           // :45
            tset(s, "created_on_pause", tget(args, "overlay_menu")); // :46
            tset(s, "created_on_state", tget(args, "state"));        // :47
            tsetn(s, "sfx_handled", 0);                              // :48
            tsetn(s, "transition_timer", 0);                         // :49
            SET_SFX(s, args);                                        // :50
            love_audio_play(snd(s));                                 // :51
            return s;                                                // :52
        }
    }

    bool should_stream = str_find(sound_code, "music", 1) || str_find(sound_code, "ambient", 1); // :56
    Table *s = TAB("sound", P(love_audio_newSource(fmt("resources/sounds/%s.ogg", sound_code),
                                                   should_stream ? "stream" : "static"))); // :57
    tinsert(ttab(SOURCES, sound_code), T(s)); // :58
    tsets(s, "sound_code", sound_code);       // :59
    Val per = tget(args, "per"), vol = tget(args, "vol");
    tset(s, "original_pitch", truthy(per) ? per : N(1));        // :60
    tset(s, "original_volume", truthy(vol) ? vol : N(1));       // :61
    tsetb(s, "created_on_pause", tbool(args, "overlay_menu"));  // :62
    tset(s, "created_on_state", tget(args, "state"));           // :63
    tsetn(s, "sfx_handled", 0);                                 // :64
    tsetn(s, "transition_timer", 0);                            // :65
    SET_SFX(s, args);                                           // :66
    love_audio_play(snd(s));                                    // :67
    return s;                                                   // :68
}

// sound_manager.lua:71
void STOP_AUDIO(void) {
    for (TIter it = titer(SOURCES); tnext(&it);) {            // :72
        for (TIter jt = titer(it.val.tab); tnext(&jt);) {     // :73
            Table *s = jt.val.tab;
            if (Source_isPlaying(snd(s))) { // :74
                Source_stop(snd(s));        // :75
            }
        }
    }
}

// sound_manager.lua:81
void SET_SFX(Table *s, Table *args) {
    Table *sound_settings = ttab(args, "sound_settings");
    if (str_find(tstr(s, "sound_code"), "music", 1)) { // :82
        Val cv = tget(s, "current_volume");
        num dt;
        if (veq(tget(s, "sound_code"), tget(args, "desired_track"))) { // :83
            if (!truthy(cv)) cv = N(1);                                // :84
            dt = arith(args, "dt");
            tsetn(s, "current_volume", 1 * (dt * 3) + (1 - (dt * 3)) * vnum(cv)); // :85
        } else {
            if (!truthy(cv)) cv = N(0);                                // :87
            dt = arith(args, "dt");
            tsetn(s, "current_volume", 0 * (dt * 3) + (1 - (dt * 3)) * vnum(cv)); // :88
        }
        if (!sound_settings) ERROR("attempt to index field 'sound_settings' (a nil value)");
        Source_setVolume(snd(s), (float)(arith(s, "current_volume") * arith(s, "original_volume") *
                                         (arith(sound_settings, "volume") / 100.0) *
                                         (arith(sound_settings, "music_volume") / 100.0))); // :90
        Source_setPitch(snd(s), (float)(arith(s, "original_pitch") * arith(args, "pitch_mod"))); // :91
    } else {
        if (!veq(tget(s, "temp_pitch"), tget(s, "original_pitch"))) {  // :93
            Source_setPitch(snd(s), (float)arith(s, "original_pitch")); // :94
            tset(s, "temp_pitch", tget(s, "original_pitch"));          // :95
        }
        if (!sound_settings) ERROR("attempt to index field 'sound_settings' (a nil value)");
        num sound_vol = arith(s, "original_volume") * (arith(sound_settings, "volume") / 100.0) *
                        (arith(sound_settings, "game_sounds_volume") / 100.0); // :97
        if (veq(tget(s, "created_on_state"), N(13))) sound_vol = sound_vol * arith(args, "splash_vol"); // :98
        if (sound_vol <= 0) {       // :99
            Source_stop(snd(s));    // :100
        } else {
            Source_setVolume(snd(s), (float)sound_vol); // :102
        }
    }
}

// sound_manager.lua:107
void MODULATE(Table *args) {
    // note: pairs(SOURCES) is in insertion order here; the order only decides which music key trips the restart.
    // The walk of every sound code with a search for "music" in each, every frame, is done once per state of
    // SOURCES' keys (its stamp, core.h): the keys that contain "music", in the walk's order, are kept.
    static Table *music_keys, *music_of;
    static uint64_t music_stamp;
    if (music_of != SOURCES || music_stamp != tstamp(SOURCES) || !music_keys) {
        music_keys = tnew();
        for (TIter it = titer(SOURCES); tnext(&it);)
            if (str_find(vstr(it.key), "music", 1)) tinsert(music_keys, S(vstr(it.key)));
        music_of = SOURCES, music_stamp = tstamp(SOURCES);
    }
    for (int mk = 1; mk <= tlen(music_keys); mk++) { // :108
        const char *k = tstri(music_keys, mk);
        Table *v = tget_k(SOURCES, k).tab;
        if (!veq(tget(args, "desired_track"), S(""))) { // :109 (the key has "music")
            Table *v1 = ttabi(v, 1);
            if (v1 && truthy(tget(v1, "sound")) && Source_isPlaying(snd(v1))) { // :110
            } else {
                RESTART_MUSIC(args); // :112
                break;               // :113
            }
        }
    }
    for (TIter it = titer(SOURCES); tnext(&it);) { // :117
        Table *v = it.val.tab;
        int i = 1;             // :118
        while (i <= tlen(v)) { // :119
            if (!Source_isPlaying(snd(ttabi(v, i)))) { // :120
                Source_release(snd(ttabi(v, i)));      // :121
                tremove(v, i);                         // :122
            } else {
                i = i + 1; // :124
            }
        }

        for (TIter jt = titer(v); tnext(&jt);) { // :128
            Table *s = jt.val.tab;
            if (truthy(tget(s, "sound")) && Source_isPlaying(snd(s)) && truthy(tget(s, "original_volume"))) { // :129
                SET_SFX(s, args); // :130
            }
        }
    }
}

// sound_manager.lua:136
void RESTART_MUSIC(Table *args) {
    // note: SOURCES[k] is replaced during the walk: an existing key, as Lua's pairs allows.
    for (TIter it = titer(SOURCES); tnext(&it);) { // :137
        const char *k = vstr(it.key);
        Table *v = it.val.tab;
        if (str_find(k, "music", 1)) { // :138
            for (int i = 1; truthy(tgeti(v, i)); i++) { // :139
                Source_stop(snd(ttabi(v, i)));          // :140
            }
            tsett(SOURCES, k, tnew()); // :142
            if (!args) ERROR("attempt to index local 'args' (a nil value)"); // RESTART_MUSIC() with nil, as the Lua
            tsetn(args, "per", 0.7);        // :143
            tsetn(args, "vol", 0.6);        // :144
            tsets(args, "sound_code", k);   // :145
            Table *s = PLAY_SOUND(args);    // :146
            tsetb(s, "initialized", true);  // :147
        }
    }
}

// sound_manager.lua:152
void AMBIENT(Table *args) {
    Table *ambient_control = ttab(args, "ambient_control");
    if (!ambient_control) ERROR("attempt to index field 'ambient_control' (a nil value)");
    // The sound codes that have an entry in ambient_control, in the walk's order, are kept while neither table's
    // keys were written (their stamps, core.h): four codes of 125, looked for every frame.
    static Table *amb_keys, *amb_src, *amb_ac;
    static uint64_t amb_src_stamp, amb_ac_stamp;
    if (amb_src != SOURCES || amb_ac != ambient_control || amb_src_stamp != tstamp(SOURCES) || amb_ac_stamp != tstamp(ambient_control) || !amb_keys) {
        amb_keys = tnew();
        for (TIter it = titer(SOURCES); tnext(&it);)
            if (truthy(tget_k(ambient_control, vstr(it.key)))) tinsert(amb_keys, S(vstr(it.key)));
        amb_src = SOURCES, amb_ac = ambient_control, amb_src_stamp = tstamp(SOURCES), amb_ac_stamp = tstamp(ambient_control);
    }
    for (int ak = 1; ak <= tlen(amb_keys); ak++) { // :153
        const char *k = tstri(amb_keys, ak);
        Table *v = tget_k(SOURCES, k).tab;
        Val ack = tget_k(ambient_control, k); // (k is a table's key: interned)
        if (truthy(ack)) { // :154
            Table *sound_settings = ttab(args, "sound_settings");
            if (!sound_settings) ERROR("attempt to index field 'sound_settings' (a nil value)");
            bool start_ambient = arith(ack.tab, "vol") * (arith(sound_settings, "volume") / 100.0) *
                                 (arith(sound_settings, "game_sounds_volume") / 100.0) > 0; // :155

            for (int i = 1; truthy(tgeti(v, i)); i++) { // :157
                Table *s = ttabi(v, i);
                if (truthy(tget(s, "sound")) && Source_isPlaying(snd(s)) && truthy(tget(s, "original_volume"))) { // :158
                    tset(s, "original_volume", tget(ack.tab, "vol")); // :159
                    SET_SFX(s, args);                                 // :160
                    start_ambient = false;                            // :161
                }
            }

            if (start_ambient) { // :165
                tsets(args, "sound_code", k);              // :166
                tset(args, "vol", tget(ack.tab, "vol"));   // :167
                tset(args, "per", tget(ack.tab, "per"));   // :168
                PLAY_SOUND(args);                          // :169
            }
        }
    }
}

// sound_manager.lua:175
void RESET_STATES(int state) {
    for (TIter it = titer(SOURCES); tnext(&it);) { // :176
        Table *v = it.val.tab;
        for (int i = 1; truthy(tgeti(v, i)); i++) { // :177
            tsetn(ttabi(v, i), "created_on_state", state); // :178
        }
    }
}

// sound_manager.lua:185-206: one turn of the thread's loop (CHANNEL:demand() is the call itself)
void sound_manager_request(Table *request) {
    if (request) { // :188
        const char *type = tstr(request, "type");
        if (false) {
        } else if (streq(type, "sound")) { // :190
            PLAY_SOUND(request);           // :191
        } else if (streq(type, "stop")) {  // :192
            STOP_AUDIO();                  // :193
        } else if (streq(type, "modulate")) { // :194
            MODULATE(request);                // :195
            if (truthy(tget(request, "ambient_control"))) AMBIENT(request); // :196
        } else if (streq(type, "restart_music")) { // :197
            RESTART_MUSIC(NULL);                   // :198: RESTART_MUSIC() with nil args, an error in the Lua too
        } else if (streq(type, "reset_states")) {  // :199
            for (TIter it = titer(SOURCES); tnext(&it);) { // :200
                Table *v = it.val.tab;
                for (int i = 1; truthy(tgeti(v, i)); i++) { // :201
                    tset(ttabi(v, i), "created_on_state", tget(request, "state")); // :202
                }
            }
        }
    }
}
