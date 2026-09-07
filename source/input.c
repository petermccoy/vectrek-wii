#include "input.h"
#include <wiiuse/wpad.h>
#include <string.h>

// Shield/cloak are toggle states in the original (a tap flips them, not a
// hold), so their latched value has to outlive any single poll.
static int shieldToggle = 0;
static int cloakToggle = 0;

void input_init(int screenWidth, int screenHeight) {
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, screenWidth, screenHeight);
    shieldToggle = 0;
    cloakToggle = 0;
}

void input_poll(ShipInput *out, const Camera *cam, const Loadout *lo) {
    memset(out, 0, sizeof(*out));

    u32 held = WPAD_ButtonsHeld(WPAD_CHAN_0);
    u32 down = WPAD_ButtonsDown(WPAD_CHAN_0);

    ir_t ir;
    WPAD_IR(WPAD_CHAN_0, &ir);

    // Hold B (the trigger) to turn toward the pointer and burn -- release to
    // coast on inertia, exactly like lifting a finger off the touchscreen.
    if ((held & WPAD_BUTTON_B) && ir.valid) {
        out->hasSteer = 1;
        out->steer = render_screen_to_world(cam, vec2(ir.x, ir.y));
        out->thrust = 1;
    }

    // Fire buttons map to equipped weapons in WeaponType order (A, then 1,
    // then 2) -- the same order the HUD lays its weapon buttons out in.
    int slot = 0;
    for (int wt = 0; wt < WEAPON_COUNT; wt++) {
        if (!lo->weaponEquipped[wt]) continue;
        int firing = 0;
        if (slot == 0) firing = (held & WPAD_BUTTON_A) != 0;
        else if (slot == 1) firing = (held & WPAD_BUTTON_1) != 0;
        else if (slot == 2) firing = (held & WPAD_BUTTON_2) != 0;
        out->fireHeld[wt] = firing;
        slot++;
    }

    if (down & WPAD_BUTTON_MINUS) shieldToggle = !shieldToggle;
    if (down & WPAD_BUTTON_PLUS) cloakToggle = !cloakToggle;
    out->shield = shieldToggle && loadout_has_defense(lo, DEFENSE_SHIELD);
    out->cloak = cloakToggle && loadout_has_defense(lo, DEFENSE_CLOAK);

    out->leaveOrbit = (held & WPAD_BUTTON_UP) != 0;
}
