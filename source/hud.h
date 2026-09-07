// All on-screen instrumentation: hull/energy bars, radar, weapon and defense
// buttons, the LEAVE ORBIT prompt, status line and the game-over overlay.
// Ported from Hud.kt; buttons show a fixed Wiimote key label instead of
// being touch hit-tested, since there's no touch input on Wii.
#ifndef VECTREK_HUD_H
#define VECTREK_HUD_H

#include "gameworld.h"
#include "entity.h"
#include "render.h"

typedef struct {
    int hasWeapon;
    WeaponType weapon;
    int hasDefense;
    DefenseType defense;
    float cx, cy, r;
    char keyLabel[8];
} HudBtn;

#define HUD_MAX_BTNS (WEAPON_COUNT + DEFENSE_COUNT)

typedef struct {
    HudBtn buttons[HUD_MAX_BTNS];
    int buttonCount;
    int laidOutW, laidOutH;
} Hud;

void hud_init(Hud *hud);
void hud_layout(Hud *hud, int w, int h, const Loadout *lo);

/** me may be NULL (no local ship yet). */
void hud_draw(Hud *hud, const Camera *cam, GameWorld *world, Ship *me,
              const ShipInput *input, float time, int gameOver);

#endif
