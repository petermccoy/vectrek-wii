// Main menu and outfitting bay. Ported from MainActivity.kt/LoadoutActivity.kt
// as a D-pad/A/B driven console-text UI (no touch on Wii).
#ifndef VECTREK_MENU_H
#define VECTREK_MENU_H

#include "loadout.h"

typedef enum { MENU_ACTION_NONE, MENU_ACTION_START, MENU_ACTION_OUTFIT, MENU_ACTION_EXIT } MenuAction;

void menu_main_init(void);
MenuAction menu_main_update(void);

typedef enum { LOADOUT_ACTION_NONE, LOADOUT_ACTION_SAVE, LOADOUT_ACTION_CANCEL } LoadoutAction;

void loadout_menu_init(const Loadout *initial);
/** On LOADOUT_ACTION_SAVE, *outLoadout is filled with the edited loadout. */
LoadoutAction loadout_menu_update(Loadout *outLoadout);

#endif
