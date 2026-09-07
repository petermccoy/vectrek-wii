// Wiimote -> pilot intent. IR pointer + the B trigger reproduce the
// Android original's "touch anywhere to turn and burn, release to coast"
// feel; face/plus-minus buttons cover weapons and defenses.
#ifndef VECTREK_INPUT_H
#define VECTREK_INPUT_H

#include "entity.h"
#include "render.h"

void input_init(int screenWidth, int screenHeight);

/** Fills `out` from the current Wiimote state. `cam` maps the IR pointer to
 *  a world-space aim point; `lo` decides which weapon slot each fire button
 *  maps to (equip order, same as the HUD button layout). */
void input_poll(ShipInput *out, const Camera *cam, const Loadout *lo);

#endif
