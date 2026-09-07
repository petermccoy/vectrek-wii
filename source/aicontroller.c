#include "aicontroller.h"
#include "gameworld.h"
#include <math.h>

void ai_init(Ship *ship, unsigned long seed) {
    rng_seed(&ship->ai.rnd, (unsigned int)seed);
    ship->ai.hasWanderTarget = 0;
    ship->ai.wanderTimer = 0.0f;
    ship->isAi = 1;
}

void ai_control(Ship *ship, GameWorld *world, float dt) {
    ShipInput *input = &ship->input;
    for (int w = 0; w < WEAPON_COUNT; w++) input->fireHeld[w] = 0;

    if (ship->orbitPlanet >= 0) {
        // Parked for repairs: cast off once the hull is mostly patched.
        input->leaveOrbit = ship->hull > ship->maxHull * 0.85f;
        return;
    }
    input->leaveOrbit = 0;

    Ship *enemy = NULL;
    float bestDistSq = 0.0f;
    for (int i = 0; i < world->shipCount; i++) {
        Ship *o = &world->ships[i];
        if (!o->alive || o == ship || o->cloakOn) continue;
        float d2 = vec2_dist_sq(o->pos, ship->pos);
        if (enemy == NULL || d2 < bestDistSq) {
            enemy = o;
            bestDistSq = d2;
        }
    }

    if (enemy != NULL && vec2_dist(enemy->pos, ship->pos) < 1500.0f) {
        float dist = vec2_dist(enemy->pos, ship->pos);
        // Lead the target by projectile flight time.
        Vec2 lead = vec2_add(enemy->pos, vec2_scale(enemy->vel, dist / 575.0f));
        input->hasSteer = 1;
        input->steer = lead;
        input->thrust = dist > 380.0f;
        float aimError = fabsf(angle_diff(vec2_angle(vec2_sub(lead, ship->pos)), ship->heading));
        if (dist < 780.0f && aimError < 0.22f) {
            input->fireHeld[WEAPON_PROJECTILE] = 1;
        }
        input->shield = loadout_has_defense(&ship->loadout, DEFENSE_SHIELD) &&
            ship->hull < ship->maxHull * 0.6f && dist < 700.0f;
    } else {
        ship->ai.wanderTimer -= dt;
        int needNew = !ship->ai.hasWanderTarget || ship->ai.wanderTimer <= 0.0f ||
            vec2_dist(ship->pos, ship->ai.wanderTarget) < 260.0f;
        if (needNew) {
            ship->ai.wanderTarget = vec2(
                300.0f + rng_float(&ship->ai.rnd) * (world->width - 600.0f),
                300.0f + rng_float(&ship->ai.rnd) * (world->height - 600.0f));
            ship->ai.hasWanderTarget = 1;
            ship->ai.wanderTimer = 4.0f + rng_float(&ship->ai.rnd) * 4.0f;
        }
        input->hasSteer = 1;
        input->steer = ship->ai.wanderTarget;
        input->thrust = 1;
        input->shield = 0;
    }
}
