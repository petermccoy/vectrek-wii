#include "entity.h"
#include "gameworld.h"
#include "aicontroller.h"
#include <string.h>
#include <math.h>

static void ship_try_fire(Ship *s, GameWorld *world, WeaponType w);
static void ship_restock_ammo(Ship *s, float dt);
static void ship_leave_orbit_internal(Ship *s, Planet *planet);

void ship_init(Ship *s, int id, const char *name, const Loadout *lo, Vec2 pos) {
    memset(s, 0, sizeof(*s));
    s->id = id;
    strncpy(s->name, name, SHIP_NAME_LEN - 1);
    s->loadout = *lo;
    s->stats = ship_stats_make(lo);
    s->pos = pos;
    s->vel = vec2_zero();
    s->radius = 18.0f;
    s->alive = 1;
    s->heading = 0.0f;
    s->hull = s->stats.maxHull;
    s->energy = s->stats.maxEnergy;
    s->maxHull = s->stats.maxHull;
    s->maxEnergy = s->stats.maxEnergy;
    s->hullStyle = lo->hullStyle;
    s->orbitPlanet = -1;
    for (int w = 0; w < WEAPON_COUNT; w++) {
        if (lo->weaponEquipped[w]) {
            WeaponSpec spec = weapon_spec((WeaponType)w, lo->weaponLevel[w]);
            if (spec.ammo >= 0) s->ammo[w] = spec.ammo;
        }
    }
}

void ship_update(Ship *s, GameWorld *world, float dt) {
    if (s->isAi) ai_control(s, world, dt);

    for (int i = 0; i < WEAPON_COUNT; i++) s->cooldowns[i] = maxf(0.0f, s->cooldowns[i] - dt);
    s->orbitCooldown = maxf(0.0f, s->orbitCooldown - dt);
    s->wormholeCooldown = maxf(0.0f, s->wormholeCooldown - dt);

    // Defenses stay up only while there is energy to feed them.
    s->shieldOn = s->input.shield && loadout_has_defense(&s->loadout, DEFENSE_SHIELD) && s->energy > 1.0f;
    s->cloakOn = s->input.cloak && loadout_has_defense(&s->loadout, DEFENSE_CLOAK) && s->energy > 1.0f;

    s->thrusting = 0;
    if (s->orbitPlanet >= 0) {
        // Parked: ride the orbit, mend the hull. Weapons stay live.
        Planet *planet = &world->planets[s->orbitPlanet];
        s->orbitAngle = wrap_angle(s->orbitAngle + s->orbitAngVel * dt);
        Vec2 radial = vec2_from_angle(s->orbitAngle);
        s->pos = vec2_add(planet->pos, vec2_scale(radial, s->orbitR));
        Vec2 tangent = vec2(-radial.y, radial.x);
        tangent = vec2_scale(tangent, s->orbitAngVel >= 0.0f ? 1.0f : -1.0f);
        s->vel = vec2_add(vec2_scale(tangent, fabsf(s->orbitAngVel) * s->orbitR), planet->vel);
        s->heading = vec2_angle(tangent);
        s->hull = minf(s->hull + ORBIT_REPAIR_RATE * dt, s->maxHull);
        ship_restock_ammo(s, dt);
        if (s->input.leaveOrbit) ship_leave_orbit_internal(s, planet);
    } else {
        // Free flight: turn toward the aimed point; burn once lined up.
        if (s->input.hasSteer) {
            Vec2 target = s->input.steer;
            float desired = atan2f(target.y - s->pos.y, target.x - s->pos.x);
            float diff = angle_diff(desired, s->heading);
            float maxTurn = s->stats.turnRate * dt;
            s->heading = wrap_angle(s->heading + clampf(diff, -maxTurn, maxTurn));
            if (s->input.thrust && fabsf(diff) < 1.2f && s->energy > 0.5f) {
                s->vel = vec2_add(s->vel, vec2_from_angle_len(s->heading, s->stats.thrustAccel * dt));
                s->energy -= s->stats.thrustDrain * dt;
                s->thrusting = 1;
            }
        }
        float sp = vec2_length(s->vel);
        if (sp > s->stats.maxSpeed) s->vel = vec2_scale(s->vel, s->stats.maxSpeed / sp);
    }

    float drain = 0.0f;
    if (s->shieldOn) drain += shield_drain(loadout_defense_level(&s->loadout, DEFENSE_SHIELD));
    if (s->cloakOn) drain += cloak_drain(loadout_defense_level(&s->loadout, DEFENSE_CLOAK));
    s->energy = clampf(s->energy + (s->stats.energyRegen - drain) * dt, 0.0f, s->maxEnergy);

    for (int w = 0; w < WEAPON_COUNT; w++) if (s->input.fireHeld[w]) ship_try_fire(s, world, (WeaponType)w);

    if (s->orbitPlanet < 0) s->pos = vec2_add(s->pos, vec2_scale(s->vel, dt));
}

/** Planetary stores refill magazines while parked (full in ~25s). */
static void ship_restock_ammo(Ship *s, float dt) {
    for (int w = 0; w < WEAPON_COUNT; w++) {
        if (!s->loadout.weaponEquipped[w]) continue;
        WeaponSpec spec = weapon_spec((WeaponType)w, s->loadout.weaponLevel[w]);
        if (spec.ammo < 0) continue;
        int current = s->ammo[w];
        if (current >= spec.ammo) continue;
        s->restockFrac[w] += (float)spec.ammo / RESTOCK_SECONDS * dt;
        if (s->restockFrac[w] >= 1.0f) {
            int add = (int)s->restockFrac[w];
            s->restockFrac[w] -= (float)add;
            int updated = current + add;
            s->ammo[w] = updated > spec.ammo ? spec.ammo : updated;
        }
    }
}

/** Captured by a planet: set up the parking orbit continuing our swing. */
void ship_enter_orbit(Ship *s, int planetIndex, Vec2 planetPos, Vec2 planetVel, float planetRadius) {
    s->orbitPlanet = planetIndex;
    s->orbitR = planetRadius + s->radius + 12.0f;
    Vec2 radial = vec2_normalized(vec2_sub(s->pos, planetPos));
    s->orbitAngle = vec2_angle(radial);
    s->pos = vec2_add(planetPos, vec2_scale(radial, s->orbitR));
    // Keep circling the way we were already moving around the planet.
    Vec2 tangent = vec2(-radial.y, radial.x);
    float side = vec2_dot(vec2_sub(s->vel, planetVel), tangent) >= 0.0f ? 1.0f : -1.0f;
    s->orbitAngVel = side * (ORBIT_LINEAR_SPEED / s->orbitR);
}

static void ship_leave_orbit_internal(Ship *s, Planet *planet) {
    Vec2 radial = vec2_from_angle(s->orbitAngle);
    Vec2 tangent = vec2(-radial.y, radial.x);
    tangent = vec2_scale(tangent, s->orbitAngVel >= 0.0f ? 1.0f : -1.0f);
    Vec2 kick = vec2_add(vec2_scale(tangent, ORBIT_LINEAR_SPEED * 1.4f), vec2_scale(radial, 70.0f));
    s->vel = vec2_add(kick, planet->vel);
    s->heading = vec2_angle(s->vel);
    s->orbitPlanet = -1;
    s->orbitCooldown = 2.0f;
}

static void ship_try_fire(Ship *s, GameWorld *world, WeaponType w) {
    if (!s->loadout.weaponEquipped[w]) return;
    if (s->cooldowns[w] > 0.0f) return;
    WeaponSpec spec = weapon_spec(w, s->loadout.weaponLevel[w]);
    if (spec.ammo >= 0 && s->ammo[w] <= 0) return;
    if (s->energy < spec.energyCost) return;

    if (w == WEAPON_ENERGY) {
        // Beam weapon: only fires if something is locked in range.
        if (!gameworld_fire_beam(world, s, spec)) return;
    } else {
        gameworld_spawn_shot(world, s, w, spec);
    }
    s->energy -= spec.energyCost;
    if (spec.ammo >= 0) s->ammo[w] -= 1;
    s->cooldowns[w] = spec.cooldown;
    // Firing while cloaked decloaks you (drops the toggle).
    if (s->cloakOn && w != WEAPON_MINE) {
        s->input.cloak = 0;
        s->cloakOn = 0;
    }
}

/** Apply damage; raised shields soak a fraction, paid for with energy. */
void ship_take_damage(Ship *s, GameWorld *world, float amount, int attackerId) {
    if (!s->alive) return;
    float dmg = amount;
    if (s->shieldOn) {
        float wanted = dmg * shield_absorb(loadout_defense_level(&s->loadout, DEFENSE_SHIELD));
        float soaked = minf(wanted, s->energy / 0.6f);
        s->energy = maxf(0.0f, s->energy - soaked * 0.6f);
        dmg -= soaked;
    }
    s->hull -= dmg;
    if (s->hull <= 0.0f) {
        s->hull = 0.0f;
        s->alive = 0;
        gameworld_on_ship_destroyed(world, s, attackerId);
    }
}

void shot_init(Shot *s, int id, ShotKind kind, int ownerId, Vec2 pos, Vec2 vel, float damage, float life) {
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->kind = kind;
    s->ownerId = ownerId;
    s->pos = pos;
    s->vel = vel;
    s->radius = (kind == SHOT_MINE) ? 10.0f : 5.0f;
    s->alive = 1;
    s->damage = damage;
    s->life = life;
    s->age = 0.0f;
    s->heading = vec2_angle(vel);
}

void shot_update(Shot *s, float dt) {
    s->age += dt;
    s->life -= dt;
    if (s->kind == SHOT_MINE) {
        // Dropped mines shed their initial drift and sit still.
        s->vel = vec2_scale(s->vel, 1.0f - minf(2.5f * dt, 1.0f));
    } else if (vec2_length_sq(s->vel) > 1.0f) {
        s->heading = vec2_angle(s->vel);
    }
    s->pos = vec2_add(s->pos, vec2_scale(s->vel, dt));
}
