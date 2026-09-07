#include "menu.h"
#include "text.h"
#include <wiiuse/wpad.h>
#include <string.h>
#include <stdio.h>

/** One decimal place via integer math -- sidesteps relying on float printf
 *  support in the homebrew libc. */
static void fmt1(float v, char *buf, int bufsize) {
    int tenths = (int)(v * 10.0f + (v >= 0.0f ? 0.5f : -0.5f));
    int whole = tenths / 10;
    int frac = tenths % 10;
    if (frac < 0) frac = -frac;
    snprintf(buf, bufsize, "%d.%d", whole, frac);
}

// ------------------------------------------------------------------
// Main menu
// ------------------------------------------------------------------

static int mainSel = 0;

void menu_main_init(void) {
    mainSel = 0;
    text_clear();
}

MenuAction menu_main_update(void) {
    u32 down = WPAD_ButtonsDown(WPAD_CHAN_0);
    if (down & WPAD_BUTTON_DOWN) mainSel = (mainSel + 1) % 3;
    if (down & WPAD_BUTTON_UP) mainSel = (mainSel + 2) % 3;

    text_color(TXT_CYAN);
    text_at(3, 28, "V E C T R E K");
    text_color(TXT_WHITE);
    text_at(5, 20, "old-school vector space combat -- Wii port");

    const char *items[3] = { "START PRACTICE ARENA", "OUTFITTING BAY", "EXIT" };
    for (int i = 0; i < 3; i++) {
        text_color(i == mainSel ? TXT_GREEN : TXT_WHITE);
        text_at(9 + i * 2, 24, "%c %s", i == mainSel ? '>' : ' ', items[i]);
    }
    text_color(TXT_WHITE);
    text_at(20, 18, "D-PAD: select      A: confirm      HOME: quit");

    if (down & WPAD_BUTTON_A) {
        if (mainSel == 0) return MENU_ACTION_START;
        if (mainSel == 1) return MENU_ACTION_OUTFIT;
        return MENU_ACTION_EXIT;
    }
    return MENU_ACTION_NONE;
}

// ------------------------------------------------------------------
// Outfitting bay
// ------------------------------------------------------------------

#define ROW_WEAPON0 0
#define ROW_DEFENSE0 WEAPON_COUNT
#define ROW_ENERGY (WEAPON_COUNT + DEFENSE_COUNT)
#define ROW_ENGINE (ROW_ENERGY + 1)
#define ROW_RADAR (ROW_ENGINE + 1)
#define ROW_HULL (ROW_RADAR + 1)
#define ROW_SAVE (ROW_HULL + 1)
#define ROW_CANCEL (ROW_SAVE + 1)
#define LOADOUT_ROW_COUNT (ROW_CANCEL + 1)

static Loadout editLo;
static int cursorRow;

void loadout_menu_init(const Loadout *initial) {
    editLo = *initial;
    cursorRow = 0;
    text_clear();
}

static const char *mkLabel(int lvl, char *buf, int bufsize) {
    snprintf(buf, bufsize, "MK%d", lvl + 1);
    return buf;
}

LoadoutAction loadout_menu_update(Loadout *outLoadout) {
    u32 down = WPAD_ButtonsDown(WPAD_CHAN_0);

    if (down & WPAD_BUTTON_DOWN) cursorRow = (cursorRow + 1) % LOADOUT_ROW_COUNT;
    if (down & WPAD_BUTTON_UP) cursorRow = (cursorRow - 1 + LOADOUT_ROW_COUNT) % LOADOUT_ROW_COUNT;

    if (cursorRow >= ROW_WEAPON0 && cursorRow < ROW_WEAPON0 + WEAPON_COUNT) {
        WeaponType wt = (WeaponType)(cursorRow - ROW_WEAPON0);
        if (down & WPAD_BUTTON_A) editLo.weaponEquipped[wt] = !editLo.weaponEquipped[wt];
        if (editLo.weaponEquipped[wt]) {
            if ((down & WPAD_BUTTON_RIGHT) && editLo.weaponLevel[wt] < WEAPON_INFO[wt].maxLevel) editLo.weaponLevel[wt]++;
            if ((down & WPAD_BUTTON_LEFT) && editLo.weaponLevel[wt] > 0) editLo.weaponLevel[wt]--;
        }
    } else if (cursorRow >= ROW_DEFENSE0 && cursorRow < ROW_DEFENSE0 + DEFENSE_COUNT) {
        DefenseType dt = (DefenseType)(cursorRow - ROW_DEFENSE0);
        if (down & WPAD_BUTTON_A) editLo.defenseEquipped[dt] = !editLo.defenseEquipped[dt];
        if (editLo.defenseEquipped[dt]) {
            if ((down & WPAD_BUTTON_RIGHT) && editLo.defenseLevel[dt] < DEFENSE_INFO[dt].maxLevel) editLo.defenseLevel[dt]++;
            if ((down & WPAD_BUTTON_LEFT) && editLo.defenseLevel[dt] > 0) editLo.defenseLevel[dt]--;
        }
    } else if (cursorRow == ROW_ENERGY) {
        if ((down & WPAD_BUTTON_RIGHT) && editLo.energyLevel < 3) editLo.energyLevel++;
        if ((down & WPAD_BUTTON_LEFT) && editLo.energyLevel > 0) editLo.energyLevel--;
    } else if (cursorRow == ROW_ENGINE) {
        if ((down & WPAD_BUTTON_RIGHT) && editLo.engineLevel < 2) editLo.engineLevel++;
        if ((down & WPAD_BUTTON_LEFT) && editLo.engineLevel > 0) editLo.engineLevel--;
    } else if (cursorRow == ROW_RADAR) {
        if ((down & WPAD_BUTTON_RIGHT) && editLo.radarLevel < 3) editLo.radarLevel++;
        if ((down & WPAD_BUTTON_LEFT) && editLo.radarLevel > 0) editLo.radarLevel--;
    } else if (cursorRow == ROW_HULL) {
        if (down & WPAD_BUTTON_RIGHT) editLo.hullStyle = (editLo.hullStyle + 1) % HULL_NAME_COUNT;
        if (down & WPAD_BUTTON_LEFT) editLo.hullStyle = (editLo.hullStyle + HULL_NAME_COUNT - 1) % HULL_NAME_COUNT;
    }

    int cost = loadout_cost(&editLo);
    int over = cost > LOADOUT_BUDGET;

    LoadoutAction result = LOADOUT_ACTION_NONE;
    if (cursorRow == ROW_SAVE && (down & WPAD_BUTTON_A) && !over) {
        *outLoadout = editLo;
        result = LOADOUT_ACTION_SAVE;
    }
    if ((cursorRow == ROW_CANCEL && (down & WPAD_BUTTON_A)) || (down & WPAD_BUTTON_B)) {
        result = LOADOUT_ACTION_CANCEL;
    }

    // ---- draw ----
    text_color(TXT_CYAN);
    text_at(1, 24, "OUTFITTING BAY");
    text_color(over ? TXT_RED : TXT_CYAN);
    text_at(3, 24, "POINTS  %d / %d%s", cost, LOADOUT_BUDGET, over ? "  -- OVER BUDGET" : "");

    char lvlbuf[16];
    int row = 5;
    for (int i = 0; i < WEAPON_COUNT; i++, row++) {
        int eq = editLo.weaponEquipped[i];
        text_color(cursorRow == i ? TXT_GREEN : (eq ? TXT_CYAN : TXT_WHITE));
        text_at(row, 4, "%c %-10s [%dpt +%d/mk]  %s",
                cursorRow == i ? '>' : ' ', WEAPON_INFO[i].label, WEAPON_INFO[i].cost, WEAPON_INFO[i].upgradeCost,
                eq ? mkLabel(editLo.weaponLevel[i], lvlbuf, sizeof(lvlbuf)) : "OFF");
    }
    for (int i = 0; i < DEFENSE_COUNT; i++, row++) {
        int eq = editLo.defenseEquipped[i];
        text_color(cursorRow == ROW_DEFENSE0 + i ? TXT_GREEN : (eq ? TXT_CYAN : TXT_WHITE));
        text_at(row, 4, "%c %-10s [%dpt +%d/mk]  %s",
                cursorRow == ROW_DEFENSE0 + i ? '>' : ' ', DEFENSE_INFO[i].label, DEFENSE_INFO[i].cost, DEFENSE_INFO[i].upgradeCost,
                eq ? mkLabel(editLo.defenseLevel[i], lvlbuf, sizeof(lvlbuf)) : "OFF");
    }
    row++;
    text_color(cursorRow == ROW_ENERGY ? TXT_GREEN : TXT_WHITE);
    text_at(row, 4, "%c ENERGY CELLS  [2pt/lvl]      LVL %d", cursorRow == ROW_ENERGY ? '>' : ' ', editLo.energyLevel);
    row++;
    text_color(cursorRow == ROW_ENGINE ? TXT_GREEN : TXT_WHITE);
    text_at(row, 4, "%c ENGINES       [1pt/lvl]      LVL %d", cursorRow == ROW_ENGINE ? '>' : ' ', editLo.engineLevel);
    row++;
    text_color(cursorRow == ROW_RADAR ? TXT_GREEN : TXT_WHITE);
    text_at(row, 4, "%c RADAR         [1pt/lvl]      LVL %d", cursorRow == ROW_RADAR ? '>' : ' ', editLo.radarLevel);
    row += 2;
    text_color(cursorRow == ROW_HULL ? TXT_GREEN : TXT_WHITE);
    text_at(row, 4, "%c HULL DESIGN (free): < %-8s >", cursorRow == ROW_HULL ? '>' : ' ', HULL_NAMES[editLo.hullStyle]);
    row += 2;
    text_color(cursorRow == ROW_SAVE ? TXT_GREEN : (over ? TXT_RED : TXT_WHITE));
    text_at(row, 4, "%c SAVE FIT", cursorRow == ROW_SAVE ? '>' : ' ');
    row++;
    text_color(cursorRow == ROW_CANCEL ? TXT_GREEN : TXT_WHITE);
    text_at(row, 4, "%c CANCEL", cursorRow == ROW_CANCEL ? '>' : ' ');

    // Description line for whichever row is selected.
    char desc[80];
    desc[0] = 0;
    char n1[16];
    if (cursorRow >= ROW_WEAPON0 && cursorRow < ROW_WEAPON0 + WEAPON_COUNT) {
        WeaponType wt = (WeaponType)cursorRow;
        WeaponSpec s = weapon_spec(wt, editLo.weaponLevel[wt]);
        if (s.ammo >= 0) {
            snprintf(desc, sizeof(desc), "dmg %d  ammo %d", (int)s.damage, s.ammo);
        } else {
            snprintf(desc, sizeof(desc), "beam range %d  dmg %d split  %d energy/shot",
                     (int)s.range, (int)s.damage, (int)s.energyCost);
        }
    } else if (cursorRow >= ROW_DEFENSE0 && cursorRow < ROW_DEFENSE0 + DEFENSE_COUNT) {
        DefenseType dt = (DefenseType)(cursorRow - ROW_DEFENSE0);
        if (dt == DEFENSE_SHIELD) {
            fmt1(shield_drain(editLo.defenseLevel[dt]), n1, sizeof(n1));
            snprintf(desc, sizeof(desc), "soaks %d%%  %s energy/s", (int)(shield_absorb(editLo.defenseLevel[dt]) * 100.0f), n1);
        } else {
            fmt1(cloak_drain(editLo.defenseLevel[dt]), n1, sizeof(n1));
            snprintf(desc, sizeof(desc), "invisible to ships & missiles  %s energy/s", n1);
        }
    } else if (cursorRow == ROW_ENERGY) {
        fmt1(6.0f + 1.5f * editLo.energyLevel, n1, sizeof(n1));
        snprintf(desc, sizeof(desc), "max %d energy  regen %s/s", 100 + 25 * editLo.energyLevel, n1);
    } else if (cursorRow == ROW_ENGINE) {
        snprintf(desc, sizeof(desc), "+%d%% thrust & turn rate", editLo.engineLevel * 15);
    } else if (cursorRow == ROW_RADAR) {
        snprintf(desc, sizeof(desc), "radar range %d", 900 + 700 * editLo.radarLevel);
    }
    text_color(TXT_WHITE);
    text_at(23, 4, "%-72s", desc);

    text_at(26, 4, "D-PAD: move/adjust   A: toggle/save   B: cancel");

    return result;
}
