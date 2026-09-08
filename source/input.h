// Wiimote -> pilot intent.
//
// Single-player uses the IR pointer + the B trigger to reproduce the
// Android original's "touch anywhere to turn and burn, release to coast"
// feel. Split-screen multiplayer can't: each Wiimote's IR reading is an
// absolute position on the one physical screen/sensor bar, which doesn't
// know which quadrant belongs to which player. Multiplayer instead uses
// direct D-pad turn-and-burn, one controller per player.
#ifndef VECTREK_INPUT_H
#define VECTREK_INPUT_H

#include "entity.h"
#include "render.h"

#define INPUT_MAX_PLAYERS 4

void input_init(int screenWidth, int screenHeight);

/** Fills `out` from Wiimote 0's state. `cam` maps the IR pointer to a
 *  world-space aim point; `lo` decides which weapon slot each fire button
 *  maps to (equip order, same as the HUD button layout). */
void input_poll(ShipInput *out, const Camera *cam, const Loadout *lo);

/** Fills `out` from Wiimote `chan`'s state (0..INPUT_MAX_PLAYERS-1) using
 *  the D-pad turn-and-burn scheme for split-screen multiplayer. */
void input_poll_mp(int chan, ShipInput *out, const Loadout *lo);

/** True if a Wiimote is currently paired on channel `chan`. */
int input_wpad_connected(int chan);

#endif
