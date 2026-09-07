// Obstacles and gravity wells that litter the board. Ported from Scenery.kt.
#ifndef VECTREK_SCENERY_H
#define VECTREK_SCENERY_H

#include "vec2.h"
#include "rng.h"

#define ASTEROID_SHAPE_VERTS 10
#define MAX_ASTEROIDS 54
#define MAX_STARS 2
#define MAX_PLANETS 8
#define MAX_WORMHOLES 2
#define MAX_EXPLOSIONS 48
#define MAX_BEAMS 16

typedef struct {
    Vec2 pos;
    float radius;
    float shape[ASTEROID_SHAPE_VERTS];
    Vec2 vel;
} Asteroid;

/** Irregular polygon: per-vertex radius multipliers. */
static inline void asteroid_make_shape(Rng *rnd, float *out, int verts) {
    for (int i = 0; i < verts; i++) out[i] = 0.72f + rng_float(rnd) * 0.42f;
}

typedef struct {
    Vec2 pos;
    float radius;
    float mass;
} Star;

typedef struct {
    int starIndex;
    float orbitRadius;
    float orbitAngle;
    float orbitSpeed;   // rad/s, signed
    float radius;
    float mass;
    Vec2 pos;
    Vec2 vel;            // instantaneous velocity, so orbiting ships inherit it
} Planet;

void planet_update(Planet *p, Vec2 starPos, float dt);

/**
 * Paired portals. Anything crossing the horizon is flung out of another
 * wormhole on the board, velocity intact. They still pull like gravity wells.
 */
typedef struct {
    Vec2 pos;
    float horizon;
    float mass;
} Wormhole;

/** Short-lived visual effect; purely cosmetic. */
typedef struct {
    Vec2 pos;
    float size;
    float age;
    int alive;
} Explosion;
#define EXPLOSION_DURATION 0.75f

/** Phaser beam flash: drawn from muzzle to each locked target for an instant. */
typedef struct {
    Vec2 from, to;
    float age;
    int alive;
} Beam;
#define BEAM_DURATION 0.22f

#endif
