// Ships and shots. Ported from Entity.kt.
#ifndef VECTREK_ENTITY_H
#define VECTREK_ENTITY_H

#include "vec2.h"
#include "loadout.h"
#include "rng.h"

typedef struct GameWorld GameWorld; // forward decl, defined in gameworld.h

#define SHIP_NAME_LEN 24

/**
 * Per-tick pilot intent. Filled by the local controller (pointer + buttons),
 * or an AI brain for practice drones.
 */
typedef struct {
    int hasSteer;
    Vec2 steer;                 // world-space point the pilot is aiming at
    int thrust;                 // burn while turning toward the steer point
    // Split-screen multiplayer has no absolute pointer that maps to any one
    // player's quadrant, so it steers with these instead: turn left/right at
    // the ship's full turn rate while thrust burns independently. Ignored
    // whenever hasSteer is set (single-player's IR-pointer aiming wins).
    int turnLeft, turnRight;
    int fireHeld[WEAPON_COUNT];
    int shield;                 // toggle states, not momentary
    int cloak;
    int leaveOrbit;             // held briefly after the LEAVE ORBIT press
} ShipInput;

/** Practice-drone brain state (one per AI-controlled ship). */
typedef struct {
    Rng rnd;
    int hasWanderTarget;
    Vec2 wanderTarget;
    float wanderTimer;
} AIState;

typedef struct {
    int id;
    char name[SHIP_NAME_LEN];
    Loadout loadout;
    ShipStats stats;

    Vec2 pos, vel;
    float radius;
    int alive;

    float heading;
    float hull, energy, maxHull, maxEnergy;
    int hullStyle;

    int ammo[WEAPON_COUNT];
    float cooldowns[WEAPON_COUNT];
    float restockFrac[WEAPON_COUNT];
    int shieldOn, cloakOn, thrusting;
    int kills;

    // Planet orbit: while captured, the ship rides a circular parking orbit
    // and slowly repairs its hull.
    int orbitPlanet;         // index into world->planets, -1 = none
    float orbitAngle, orbitR, orbitAngVel;
    /** Post-departure grace: no re-capture and no gravity, so casting off
     *  doesn't slide the ship down into the planet's sun. */
    float orbitCooldown;

    /**
     * Grace period after a wormhole transit: no wormhole can grab us and
     * gravity leaves us alone, so we can actually climb out of the well.
     */
    float wormholeCooldown;

    ShipInput input;
    int isAi;
    AIState ai;
} Ship;

static inline int ship_in_orbit(const Ship *s) { return s->orbitPlanet >= 0; }
static inline float ship_cooldown(const Ship *s, WeaponType w) { return s->cooldowns[w]; }

void ship_init(Ship *s, int id, const char *name, const Loadout *lo, Vec2 pos);
void ship_update(Ship *s, GameWorld *world, float dt);
void ship_enter_orbit(Ship *s, int planetIndex, Vec2 planetPos, Vec2 planetVel, float planetRadius);
/** Apply damage; raised shields soak a fraction, paid for with energy. */
void ship_take_damage(Ship *s, GameWorld *world, float amount, int attackerId);

#define ORBIT_REPAIR_RATE 5.0f      // hull per second while parked
#define ORBIT_LINEAR_SPEED 85.0f    // parking-orbit tangential speed
#define RESTOCK_SECONDS 25.0f       // full magazine refill time

typedef enum { SHOT_SLUG, SHOT_MISSILE, SHOT_MINE } ShotKind;

typedef struct {
    int id;
    ShotKind kind;
    int ownerId;
    Vec2 pos, vel;
    float radius;
    int alive;
    float damage;
    float life;
    float age;
    float heading;
} Shot;

#define MINE_ARM_TIME 1.2f
#define MINE_TRIGGER_RADIUS 95.0f
#define MINE_BLAST_RADIUS 150.0f

static inline int shot_armed(const Shot *s) { return s->kind != SHOT_MINE || s->age > MINE_ARM_TIME; }

void shot_init(Shot *s, int id, ShotKind kind, int ownerId, Vec2 pos, Vec2 vel, float damage, float life);
void shot_update(Shot *s, float dt);

#endif
