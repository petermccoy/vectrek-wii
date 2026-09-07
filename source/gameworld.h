// The whole battlefield. Ported from GameWorld.kt.
#ifndef VECTREK_GAMEWORLD_H
#define VECTREK_GAMEWORLD_H

#include "vec2.h"
#include "rng.h"
#include "loadout.h"
#include "entity.h"
#include "scenery.h"

#define MAX_SHIPS 8
#define MAX_SHOTS 220

struct GameWorld {
    float width, height;

    Ship ships[MAX_SHIPS];
    int shipCount;

    Shot shots[MAX_SHOTS];
    int shotCount;

    Asteroid asteroids[MAX_ASTEROIDS];
    int asteroidCount;

    Star stars[MAX_STARS];
    int starCount;

    Planet planets[MAX_PLANETS];
    int planetCount;

    Wormhole wormholes[MAX_WORMHOLES];
    int wormholeCount;

    Explosion explosions[MAX_EXPLOSIONS];
    int explosionCount;

    Beam beams[MAX_BEAMS];
    int beamCount;

    long tick;
    int nextShotId;
    int nextShipId;
    Rng rnd;
};

/** Post-transit window: no re-entry and no gravity on the ship. */
#define WORMHOLE_GRACE 2.0f
#define MIN_WORMHOLE_EXIT_SPEED 180.0f

void gameworld_init(GameWorld *w, unsigned long seed, float width, float height);
void gameworld_generate(GameWorld *w);
Vec2 gameworld_find_spawn_point(GameWorld *w, const Vec2 *awayFrom);
Ship *gameworld_add_ship(GameWorld *w, const char *name, const Loadout *lo, const Vec2 *pos);

void gameworld_step(GameWorld *w, float dt);

/** Called by entity.c while resolving pilot fire intent. */
void gameworld_spawn_shot(GameWorld *w, Ship *owner, WeaponType wt, WeaponSpec spec);
int gameworld_fire_beam(GameWorld *w, Ship *owner, WeaponSpec spec);
void gameworld_on_ship_destroyed(GameWorld *w, Ship *ship, int attackerId);
void gameworld_add_explosion(GameWorld *w, Vec2 pos, float size);

#endif
