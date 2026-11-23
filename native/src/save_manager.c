// engine/save_manager.lua:1-84. The Lua is a thread looping on a channel; here each request runs inline (io.h).
#include "all.h"

// save_manager.lua:1-12: requires, the jit.off() for OS X arm and CHANNEL: nothing to translate (no thread, no channel).

// save_manager.lua:20-22, :64-66, :77-79: (profile or 1)..'' , create the directory if missing, then add '/'
static const char *prefix_profile_of(Val profile) {
    const char *prefix_profile = vstr(truthy(profile) ? profile : N(1));
    if (!prefix_profile) ERROR("attempt to concatenate a %s value", lua_typename_of(profile));
    if (!love_filesystem_getInfo(prefix_profile, NULL)) love_filesystem_createDirectory(prefix_profile);
    return fmt("%s/", prefix_profile);
}

// save_manager.lua:29-31: meta.<key> = meta.<key> or {}
static Table *meta_sub(Table *meta, const char *key) {
    Val v = tget(meta, key);
    if (!truthy(v)) { v = T(tnew()); tset(meta, key, v); }
    if (v.t != V_TAB) ERROR("attempt to index field '%s' (a %s value)", key, lua_typename_of(v));
    return v.tab;
}

// save_manager.lua:19 'save_progress'
void save_manager_save_progress(Table *save_progress) {
    const char *prefix_profile = prefix_profile_of(tget(ttab(save_progress, "SETTINGS"), "profile")); // :20-22

    const char *meta_file = fmt("%smeta.jkr", prefix_profile);
    if (!love_filesystem_getInfo(meta_file, NULL)) { // :24
        love_filesystem_append(meta_file, "return {}", 9);
    }

    const char *packed = get_compressed(meta_file); // :28
    Table *meta = STR_UNPACK(packed ? packed : "return {}");
    Table *unlocked = meta_sub(meta, "unlocked");     // :29
    Table *discovered = meta_sub(meta, "discovered"); // :30
    Table *alerted = meta_sub(meta, "alerted");       // :31

    bool _append = false; // :33

    // note: pairs() order; the merge is order-independent (spec, engine-io.md)
    for (TIter it = titer(ttab(save_progress, "UDA")); tnext(&it);) { // :35
        Val k = it.key;
        const char *v = vstr(it.val);
        if (!v) ERROR("bad argument #1 to 'find' (string expected, got %s)", lua_typename_of(it.val));
        // note: 'u', 'd', 'a' have no pattern characters: a plain find is the same search
        if (str_find(v, "u", 1) && !truthy(tgetv(unlocked, k))) { // :36
            tsetv(unlocked, k, B(true));
            _append = true;
        }
        if (str_find(v, "d", 1) && !truthy(tgetv(discovered, k))) { // :40
            tsetv(discovered, k, B(true));
            _append = true;
        }
        if (str_find(v, "a", 1) && !truthy(tgetv(alerted, k))) { // :44
            tsetv(alerted, k, B(true));
            _append = true;
        }
    }
    // note: compress_and_save_table is compress_and_save(file, STR_PACK(t)) without keeping the text (io.h)
    if (_append) compress_and_save_table(meta_file, meta); // :49

    compress_and_save_table("settings.jkr", ttab(save_progress, "SETTINGS")); // :51
    compress_and_save_table(fmt("%sprofile.jkr", prefix_profile), ttab(save_progress, "PROFILE")); // :52

    // :54 CHANNEL:push('done'): no channel; the Lua's own loop takes it back and ignores it
}

// save_manager.lua:56 'save_settings'
void save_manager_save_settings(Table *save_settings, num profile_num, Table *save_profile) {
    compress_and_save_table("settings.jkr", save_settings); // :57
    compress_and_save_table(fmt("%s/profile.jkr", number_tostring(profile_num)), save_profile); // :58
}

// save_manager.lua:60 'save_metrics'
void save_manager_save_metrics(Table *save_metrics) {
    compress_and_save_table("metrics.jkr", save_metrics); // :61
}

// save_manager.lua:63 'save_notify'
void save_manager_save_notify(const char *save_notify, Val profile_num) {
    const char *prefix_profile = prefix_profile_of(profile_num); // :64-66

    const char *file = fmt("%sunlock_notify.jkr", prefix_profile);
    if (!love_filesystem_getInfo(file, NULL)) love_filesystem_append(file, "", 0); // :68
    const char *unlock_notify = get_compressed(file); // :69
    if (!unlock_notify) unlock_notify = "";

    // note: the Lua's find is a pattern search; the keys (j_joker, b_red, ...) have no pattern characters, so a
    // plain substring search over the whole text is the same (spec, parity trap 13)
    if (save_notify && !str_find(unlock_notify, save_notify, 1)) { // :71
        compress_and_save(file, fmt("%s%s\n", unlock_notify, save_notify));
    }
}

// save_manager.lua:76 'save_run'
void save_manager_save_run(Table *save_table, Val profile_num) {
    const char *prefix_profile = prefix_profile_of(profile_num); // :77-79

    compress_and_save_table(fmt("%ssave.jkr", prefix_profile), save_table); // :81
}

// save_manager.lua:14-83: one turn of the loop
void save_manager_request(Table *request) {
    if (request) { // :17
        const char *type = tstr(request, "type");
        if (streq(type, "save_progress")) { // :19
            save_manager_save_progress(ttab(request, "save_progress"));
        } else if (streq(type, "save_settings")) { // :56
            // note: :57 runs before :58 raises on a nil profile_num, so settings.jkr is written first as in the Lua
            Val profile_num = tget(request, "profile_num");
            if (profile_num.t != V_NUM && profile_num.t != V_STR) {
                compress_and_save_table("settings.jkr", ttab(request, "save_settings"));
                ERROR("attempt to concatenate field 'profile_num' (a %s value)", lua_typename_of(profile_num));
            }
            save_manager_save_settings(ttab(request, "save_settings"), vnum(profile_num), ttab(request, "save_profile"));
        } else if (streq(type, "save_metrics")) { // :60
            save_manager_save_metrics(ttab(request, "save_metrics"));
        } else if (streq(type, "save_notify")) { // :63
            save_manager_save_notify(tstr(request, "save_notify"), tget(request, "profile_num"));
        } else if (streq(type, "save_run")) { // :76
            save_manager_save_run(ttab(request, "save_table"), tget(request, "profile_num"));
        }
    }
}
