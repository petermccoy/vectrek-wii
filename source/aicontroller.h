// Practice-drone brain. Ported from AIController.kt.
#ifndef VECTREK_AICONTROLLER_H
#define VECTREK_AICONTROLLER_H

#include "entity.h"

/** Seeds a ship's embedded AIState (call once after ship_init, before use). */
void ai_init(Ship *ship, unsigned long seed);

/** Wander the arena; chase and fire on the nearest visible non-cloaked ship. */
void ai_control(Ship *ship, GameWorld *world, float dt);

#endif
