#include "input.h"
#include <wiiuse/wpad.h>
#include <string.h>

// Shield/cloak are toggle states in the original (a tap flips them, not a
// hold), so their latched value has to outlive any single poll -- one per
// Wiimote channel, since up to 4 play at once in split-screen.
static int shieldToggle[INPUT_MAX_PLAYERS];
static int cloakToggle[INPUT_MAX_PLAYERS];

void input_init(int screenWidth, int screenHeight) {
    WPAD_Init();
    for (int chan = 0; chan < INPUT_MAX_PLAYERS; chan++) {
        WPAD_SetDataFormat(chan, WPAD_FMT_BTNS_ACC_IR);
        WPAD_SetVRes(chan, screenWidth, screenHeight);
    }
    memset(shieldToggle, 0, sizeof(shieldToggle));
    memset(cloakToggle, 0, sizeof(cloakToggle));
}

int input_wpad_connected(int chan) {
    u32 type;
    return WPAD_Probe(chan, &type) == WPAD_ERR_NONE;
}

/** Fire buttons map to equipped weapons in WeaponType order, matching the
 *  order the HUD lays its weapon buttons out in. */
static void poll_fire_buttons(ShipInput *out, const Loadout *lo, u32 held,
                               u32 slot0Mask, u32 slot1Mask, u32 slot2Mask) {
    int slot = 0;
    for (int wt = 0; wt < WEAPON_COUNT; wt++) {
        if (!lo->weaponEquipped[wt]) continue;
        int firing = 0;
        if (slot == 0) firing = (held & slot0Mask) != 0;
        else if (slot == 1) firing = (held & slot1Mask) != 0;
        else if (slot == 2) firing = (held & slot2Mask) != 0;
        out->fireHeld[wt] = firing;
        slot++;
    }
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

    poll_fire_buttons(out, lo, held, WPAD_BUTTON_A, WPAD_BUTTON_1, WPAD_BUTTON_2);

    if (down & WPAD_BUTTON_MINUS) shieldToggle[0] = !shieldToggle[0];
    if (down & WPAD_BUTTON_PLUS) cloakToggle[0] = !cloakToggle[0];
    out->shield = shieldToggle[0] && loadout_has_defense(lo, DEFENSE_SHIELD);
    out->cloak = cloakToggle[0] && loadout_has_defense(lo, DEFENSE_CLOAK);

    out->leaveOrbit = (held & WPAD_BUTTON_UP) != 0;
}

void input_poll_mp(int chan, ShipInput *out, const Loadout *lo) {
    memset(out, 0, sizeof(*out));
    if (chan < 0 || chan >= INPUT_MAX_PLAYERS) return;

    u32 held = WPAD_ButtonsHeld(chan);
    u32 down = WPAD_ButtonsDown(chan);

    // Direct turn-and-burn: D-pad left/right rotates, D-pad up thrusts,
    // D-pad down leaves orbit -- no IR pointer involved.
    out->turnLeft = (held & WPAD_BUTTON_LEFT) != 0;
    out->turnRight = (held & WPAD_BUTTON_RIGHT) != 0;
    out->thrust = (held & WPAD_BUTTON_UP) != 0;
    out->leaveOrbit = (held & WPAD_BUTTON_DOWN) != 0;

    poll_fire_buttons(out, lo, held, WPAD_BUTTON_A | WPAD_BUTTON_B, WPAD_BUTTON_1, WPAD_BUTTON_2);

    if (down & WPAD_BUTTON_MINUS) shieldToggle[chan] = !shieldToggle[chan];
    if (down & WPAD_BUTTON_PLUS) cloakToggle[chan] = !cloakToggle[chan];
    out->shield = shieldToggle[chan] && loadout_has_defense(lo, DEFENSE_SHIELD);
    out->cloak = cloakToggle[chan] && loadout_has_defense(lo, DEFENSE_CLOAK);
}
