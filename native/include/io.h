// engine/sound_manager.lua, engine/save_manager.lua, engine/string_packer.lua: sound, saving, packing tables.
// In the Lua the first two are threads fed through a channel. Here there is no thread and no channel: a request
// is executed at once, by the function that would have pushed it (sound_manager_request, save_manager_request).
// None of these files defines a class: everything here is plain functions over Tables (docs/conventions.md, "Tables").
#ifndef IO_H
#define IO_H
#include "types.h"
#include "node.h"

// ================================================================ engine/sound_manager.lua
// Implemented in src/sound_manager.c. These ARE the sound functions of the port (the F_SOUND_THREAD = true
// behaviour, run inline).
// note: functions/misc_functions.lua:1892-2022 defines globals with the same seven names (PLAY_SOUND ... RESET_STATES),
// the main thread's fallback for F_SOUND_THREAD = false, which is never taken. They are NOT translated: defining them
// again in misc_functions.c is a duplicate symbol. On the PC those copies work on the main thread's own SOURCES
// (game.lua:64-73, lists that stay empty), so the only live call to one, RESET_STATES(G.STATES.MENU) at game.lua:1524,
// does nothing there: to stay literal that call site must not reach the RESET_STATES below (leave a comment).
//
// SOURCES (sound_manager.lua:14): sound_code -> array of voice records. Never NULL after sound_manager_init().
// A voice record is a Table with the Lua's keys:
//   sound             P(Source *)  the platform's source (love.h); read it with tget(s, "sound").p
//   filepath          string       only on the voices sound_manager_init preloads; never read
//   sound_code        string       its key in SOURCES ("music1", "ambientFire2", "card1"...)
//   original_pitch    num          args.per at play (default 1)
//   original_volume   num          args.vol at play (default 1); absent on a preloaded voice until it is played;
//                                  AMBIENT rewrites it every frame for the ambient codes
//   created_on_pause  bool         args.overlay_menu at play; never read
//   created_on_state  num          args.state at play (a G.STATES value); only compared with 13 (SPLASH)
//   sfx_handled, transition_timer  num 0; never read
//   current_volume    num 0..1     music only: the crossfade weight; absent until the first SET_SFX
//   temp_pitch        num          not music: the pitch last applied; absent until the first SET_SFX
//   initialized       bool         true on the voices RESTART_MUSIC starts; never read
extern Table *SOURCES;
extern bool DISABLE_SFX; // sound_manager.lua:11: false, never read
// (CHANNEL and LOAD_CHANNEL do not exist. G.SOUND_MANAGER / G.SAVE_MANAGER, if game.h keeps them, hold nothing.)

// The args of the functions below is the request table the main thread builds, used in place (the Lua's channel
// copies it; nothing depends on the copy). Keys:
//   type            "sound" | "modulate" | "stop" | "restart_music" | "reset_states"  (sound_manager_request only)
//   sound_code      string  what to play (PLAY_SOUND; RESTART_MUSIC and AMBIENT write it)
//   per, vol        num or nil: pitch and volume, default 1 each (PLAY_SOUND writes the default back into args)
//   pitch_mod       num     G.PITCH_MOD (music pitch)
//   state           num     G.STATE
//   sound_settings  Table   G.SETTINGS.SOUND: {volume, music_volume, game_sounds_volume}, 0..100 each
//   splash_vol      num     G.SPLASH_VOL (nil on a "sound" request sent before the first modulate_sound)
//   overlay_menu    bool
//   dt              num     "modulate" only; the music branch of SET_SFX needs it
//   desired_track   string  "modulate" only: "", "music1".."music5" (or G.video_soundtrack)
//   ambient_control Table   "modulate" only: sound_code -> {per = num, vol = num}
//   time, crt, music_control: sent, never read

// sound_manager.lua:1-35 and :183, the thread's start: fills SOURCES with one preloaded voice per .ogg of
// resources/sounds. Call it where game.lua:87 starts the thread (after game.lua:64-73, which also assigns SOURCES).
void sound_manager_init(void);
// sound_manager.lua:37: args never NULL. Returns the voice record it started (never NULL).
Table *PLAY_SOUND(Table *args);
// sound_manager.lua:71
void STOP_AUDIO(void);
// sound_manager.lua:81: s a voice record, args as above; neither NULL.
void SET_SFX(Table *s, Table *args);
// sound_manager.lua:107: args never NULL. Does not call AMBIENT (sound_manager_request does).
void MODULATE(Table *args);
// sound_manager.lua:136: args never NULL (the Lua's RESTART_MUSIC() with nil, UI_definitions.lua:2848 and the
// 'restart_music' request, is an error there: pass NULL and it raises ERROR as the Lua does).
void RESTART_MUSIC(Table *args);
// sound_manager.lua:152: args never NULL and args.ambient_control present.
void AMBIENT(Table *args);
// sound_manager.lua:175: state a G.STATES value. Never called by the Lua's thread (see the note above).
void RESET_STATES(int state);
// sound_manager.lua:185-206, one turn of the thread's loop: what G.SOUND_MANAGER.channel:push(request) becomes
// (misc_functions.lua:712 and :776, main.lua:122). Dispatches on request.type; "modulate" runs MODULATE and then
// AMBIENT if request.ambient_control. request never NULL.
void sound_manager_request(Table *request);

// ================================================================ engine/save_manager.lua
// Implemented in src/save_manager.c. The Lua has no named function: these are the branches of its loop. Each one
// writes its files before it returns (the tables are packed at the call, as the channel's copy fixes them at the push).
// Files, under the save directory (N = the profile number as text, "1".."3"): settings.jkr, metrics.jkr,
// N/profile.jkr, N/meta.jkr, N/save.jkr, N/unlock_notify.jkr.

// save_manager.lua:14-83, one turn of the loop: what G.SAVE_MANAGER.channel:push(request) becomes (game.lua:1218,
// :2693, :2700, :2705, :2714). Dispatches on request.type to the five functions below, taking their arguments from
// the request's keys of the same names. request never NULL.
void save_manager_request(Table *request);
// save_manager.lua:19 'save_progress'. save_progress: {UDA = {key -> flags string of 'u', 'd', 'a'},
// SETTINGS = G.SETTINGS, PROFILE = G.PROFILES[n]}; never NULL. The profile is SETTINGS.profile, default 1.
void save_manager_save_progress(Table *save_progress);
// save_manager.lua:56 'save_settings'. profile_num has no default (the Lua errors on nil); the tables never NULL.
void save_manager_save_settings(Table *save_settings, num profile_num, Table *save_profile);
// save_manager.lua:60 'save_metrics'. Never NULL.
void save_manager_save_metrics(Table *save_metrics);
// save_manager.lua:63 'save_notify'. save_notify: the key to append, NULL as nil (then nothing is written, but the
// directory and the file are still created). profile_num: NIL as nil, default 1.
void save_manager_save_notify(const char *save_notify, Val profile_num);
// save_manager.lua:76 'save_run'. save_table never NULL. profile_num: NIL as nil, default 1.
void save_manager_save_run(Table *save_table, Val profile_num);

// ================================================================ engine/string_packer.lua
// Implemented in src/string_packer.c. The packed form of a table is the Lua's own: the text "return {[k]=v,...}" of
// string_packer.lua:22-47 (a C string, no NUL inside), and STR_UNPACK reads that grammar: callers pass the literal
// "return {}" for a missing file (game.lua:740, save_manager.lua:28) and saves of the PC load. A file on disk is that
// text (or any text) deflated; get_compressed also takes a file that starts with "return" as not compressed.

// string_packer.lua:22: data never NULL; recursive: nil is false (true leaves out the leading "return "). A value
// that is an object (V_OBJ: the Lua's v:is(Object)) is written as the string "MANUAL_REPLACE". A table as a key is
// an ERROR. The text returned lives forever (docs/conventions.md): for a big table that is only going to a file use
// compress_and_save_table, which keeps nothing.
const char *STR_PACK(Table *data, bool recursive);
// string_packer.lua:49: str never NULL (the callers apply their own `or 'return {}'`). ERROR if it does not parse.
// Returns a new table.
Table *STR_UNPACK(const char *str);
// string_packer.lua:53: the file's text, inflated if it was compressed. NULL as nil: no such file, an empty file, or
// one that does not inflate. Not freed by the caller.
const char *get_compressed(const char *_file);
// string_packer.lua:68 with _data a string: deflates the text and replaces the file.
void compress_and_save(const char *_file, const char *_data);
// string_packer.lua:68 with _data a table: compress_and_save(_file, STR_PACK(_data)) without keeping the text.
void compress_and_save_table(const char *_file, Table *_data);
// The same packing over buffers the caller owns, for code that must not leave a string behind (the two functions
// above and below are built on these). table_pack: STR_PACK(data) into a malloc'd buffer, NUL-terminated, its
// length without the NUL in *len (len NULL: not wanted); free() it. table_unpack: STR_UNPACK of len bytes that
// need not be NUL-terminated; NULL if they do not parse (no ERROR).
char *table_pack(Table *data, size_t *len);
Table *table_unpack(const char *bytes, size_t len);
#endif
