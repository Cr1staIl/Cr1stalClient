// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// Cr1stal Client: internal
MACRO_CONFIG_INT(CcPerfReset, cc_perf_reset, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: internal, anti-aliasing from old test builds was reset once")

// Cr1stal Client: updater
MACRO_CONFIG_INT(CcUpdateCheck, cc_update_check, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: check for a new version when the game starts")

// Cr1stal Client: presence and game packs
MACRO_CONFIG_INT(CcClientBadge, cc_client_badge, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: show the client avatar before the names of Cr1stal Client players")
MACRO_CONFIG_INT(CcOthersPacks, cc_others_packs, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: show other Cr1stal Client players with their own game texture pack")
MACRO_CONFIG_INT(CcSharePresence, cc_share_presence, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: let other Cr1stal Client players see that you use it and see your game pack")
MACRO_CONFIG_INT(CcStyle, cc_style, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cr1stal Client: new menu design (dark panels, neon, round shapes)")
