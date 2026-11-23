// functions/misc_functions.lua:1891-2022: the main thread's sound fallback (G.F_SOUND_THREAD = false, never taken).
#include "all.h"

// misc_functions.lua:1892 PLAY_SOUND
// misc_functions.lua:1912 STOP_AUDIO
// misc_functions.lua:1922 SET_SFX
// misc_functions.lua:1948 MODULATE
// misc_functions.lua:1977 RESTART_MUSIC
// misc_functions.lua:1993 AMBIENT
// misc_functions.lua:2016 RESET_STATES
// note: none of the seven is defined here, on purpose. misc_functions.h declares no function for this line range and
// io.h declares these same seven names as the sound manager's versions (engine/sound_manager.lua), defined in
// src/sound_manager.c only: a second definition here is a duplicate symbol at link time (misc_functions.h:19-22,
// io.h:13-17, docs/header-notes.md "Sound functions not declared here"). play_sound (:695) and modulate_sound (:719) go
// through sound_manager_request, so nothing reaches this fallback.
