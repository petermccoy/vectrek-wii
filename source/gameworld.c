#include "gameworld.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ------------------------------------------------------------------
// Forward declarations of internal helpers (defined below, used above
// their definition inside gameworld_step).
// ------------------------------------------------------------------
static int gw_is_clear(GameWorld *w, Vec2 p, float clear);
static Vec2 gw_gravity_at(GameWorld *w, Vec2 p);
static void gw_update_asteroids(GameWorld *w, float dt);
static void gw_steer_missile(GameWorld *w, Shot *shot, float dt);
static void gw_impact_damage(GameWorld *w, Ship *s, float impactSpeed);
static void gw_bounce_ship(GameWorld *w, Ship *s, Vec2 normal, Vec2 corrected);
static void gw_collide_ship_with_circle(GameWorld *w, Ship *s, Vec2 c, float r);
static void gw_collide_ship_with_world(GameWorld *w, Ship *s);
static void gw_ride_wormholes(GameWorld *w, Ship *s);
static void gw_detonate(GameWorld *w, Shot *mine);
static int gw_collide_shot_with_world(GameWorld *w, Shot *shot);
static void gw_update_effects(GameWorld *w, float dt);
static void gw_compact_shots(GameWorld *w);
static void gw_compact_ships(GameWorld *w);

// ------------------------------------------------------------------
// World generation (deterministic from seed)
// ------------------------------------------------------------------

typedef struct { Vec2 pos; float clear; } Placed;

static int gw_fits(Placed *placed, int placedCount, float width, float height, Vec2 p, float clear) {
    if (p.x < clear + 150.0f || p.x > width - clear - 150.0f) return 0;
    if (p.y < clear + 150.0f || p.y > height - clear - 150.0f) return 0;
    for (int i = 0; i < placedCount; i++) {
        if (vec2_dist(p, placed[i].pos) <= clear + placed[i].clear + 130.0f) return 0;
    }
    return 1;
}

static int gw_place(GameWorld *w, Placed *placed, int *placedCount, float clear, Vec2 *outPos) {
    for (int t = 0; t < 60; t++) {
        Vec2 p = vec2(rng_range(&w->rnd, 200.0f, w->width - 200.0f), rng_range(&w->rnd, 200.0f, w->height - 200.0f));
        if (gw_fits(placed, *placedCount, w->width, w->height, p, clear)) {
            placed[*placedCount].pos = p;
            placed[*placedCount].clear = clear;
            (*placedCount)++;
            *outPos = p;
            return 1;
        }
    }
    return 0;
}

void gameworld_init(GameWorld *w, unsigned long seed, float width, float height) {
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->nextShotId = 1;
    w->nextShipId = 1;
    w->tick = 0;
    rng_seed(&w->rnd, (unsigned int)seed);
}

void gameworld_generate(GameWorld *w) {
    Placed placed[64];
    int placedCount = 0;

    // Suns with 1-4 orbiting planets each, spread across wide orbits
    for (int i = 0; i < 2; i++) {
        float starRadius = rng_range(&w->rnd, 20.0f, 28.0f);
        int planetCount = 1 + rng_int(&w->rnd, 4);
        float maxOrbit = starRadius + 110.0f + (planetCount - 1) * 95.0f + 35.0f;
        Vec2 p;
        if (!gw_place(w, placed, &placedCount, maxOrbit + 60.0f, &p)) continue;
        if (w->starCount >= MAX_STARS) continue;
        int starIdx = w->starCount++;
        Star *star = &w->stars[starIdx];
        star->pos = p;
        star->radius = starRadius;
        star->mass = 9.5e6f;
        for (int j = 0; j < planetCount && w->planetCount < MAX_PLANETS; j++) {
            float orbitR = starRadius + 110.0f + j * 95.0f + rng_range(&w->rnd, 0.0f, 30.0f);
            Planet *pl = &w->planets[w->planetCount++];
            pl->starIndex = starIdx;
            pl->orbitRadius = orbitR;
            pl->orbitAngle = rng_range(&w->rnd, 0.0f, TWO_PI);
            pl->orbitSpeed = (rng_bool(&w->rnd) ? 1.0f : -1.0f) * rng_range(&w->rnd, 0.12f, 0.3f);
            pl->radius = rng_range(&w->rnd, 14.0f, 24.0f);
            pl->mass = 1.2e6f;
            pl->pos = vec2(star->pos.x + cosf(pl->orbitAngle) * orbitR, star->pos.y + sinf(pl->orbitAngle) * orbitR);
            pl->vel = vec2_zero();
        }
    }

    // Paired wormholes: fall into one, get flung out of the other
    for (int i = 0; i < 2; i++) {
        Vec2 p;
        if (!gw_place(w, placed, &placedCount, 300.0f, &p)) continue;
        if (w->wormholeCount >= MAX_WORMHOLES) continue;
        Wormhole *wh = &w->wormholes[w->wormholeCount++];
        wh->pos = p;
        wh->horizon = 27.0f;
        wh->mass = 1.5e7f;
    }

    // Slowly drifting asteroid field
    for (int i = 0; i < 54; i++) {
        float r = rng_range(&w->rnd, 18.0f, 45.0f);
        Vec2 p;
        if (!gw_place(w, placed, &placedCount, r + 30.0f, &p)) continue;
        if (w->asteroidCount >= MAX_ASTEROIDS) continue;
        Asteroid *a = &w->asteroids[w->asteroidCount++];
        a->pos = p;
        a->radius = r;
        asteroid_make_shape(&w->rnd, a->shape, ASTEROID_SHAPE_VERTS);
        a->vel = vec2_from_angle_len(rng_range(&w->rnd, 0.0f, TWO_PI), rng_range(&w->rnd, 8.0f, 35.0f));
    }
}

static int gw_is_clear(GameWorld *w, Vec2 p, float clear) {
    for (int i = 0; i < w->asteroidCount; i++)
        if (vec2_dist(w->asteroids[i].pos, p) < w->asteroids[i].radius + clear) return 0;
    for (int i = 0; i < w->starCount; i++)
        if (vec2_dist(w->stars[i].pos, p) < w->stars[i].radius + clear + 250.0f) return 0;
    for (int i = 0; i < w->wormholeCount; i++)
        if (vec2_dist(w->wormholes[i].pos, p) < w->wormholes[i].horizon + clear + 250.0f) return 0;
    for (int i = 0; i < w->planetCount; i++)
        if (vec2_dist(w->planets[i].pos, p) < w->planets[i].radius + clear) return 0;
    return 1;
}

/** A spawn location with breathing room from scenery and other ships. */
Vec2 gameworld_find_spawn_point(GameWorld *w, const Vec2 *awayFrom) {
    for (int t = 0; t < 200; t++) {
        Vec2 p = vec2(rng_range(&w->rnd, 300.0f, w->width - 300.0f), rng_range(&w->rnd, 300.0f, w->height - 300.0f));
        if (!gw_is_clear(w, p, 200.0f)) continue;
        int tooClose = 0;
        for (int i = 0; i < w->shipCount; i++) {
            if (w->ships[i].alive && vec2_dist(w->ships[i].pos, p) < 500.0f) { tooClose = 1; break; }
        }
        if (tooClose) continue;
        if (awayFrom != NULL && vec2_dist(p, *awayFrom) < 1200.0f) continue;
        return p;
    }
    return vec2(w->width / 2.0f, w->height / 2.0f);
}

Ship *gameworld_add_ship(GameWorld *w, const char *name, const Loadout *lo, const Vec2 *pos) {
    if (w->shipCount >= MAX_SHIPS) return NULL;
    Ship *s = &w->ships[w->shipCount++];
    Vec2 p = pos ? *pos : gameworld_find_spawn_point(w, NULL);
    ship_init(s, w->nextShipId++, name, lo, p);
    return s;
}

// ------------------------------------------------------------------
// Simulation
// ------------------------------------------------------------------

static void gw_pull(Vec2 src, float mass, Vec2 p, float *ax, float *ay) {
    float dx = src.x - p.x, dy = src.y - p.y;
    float d2 = maxf(dx * dx + dy * dy, 3600.0f);
    float d = sqrtf(d2);
    float a = minf(mass / d2, 900.0f);
    *ax += dx / d * a;
    *ay += dy / d * a;
}

static Vec2 gw_gravity_at(GameWorld *w, Vec2 p) {
    float ax = 0.0f, ay = 0.0f;
    for (int i = 0; i < w->starCount; i++) gw_pull(w->stars[i].pos, w->stars[i].mass, p, &ax, &ay);
    for (int i = 0; i < w->planetCount; i++) gw_pull(w->planets[i].pos, w->planets[i].mass, p, &ax, &ay);
    for (int i = 0; i < w->wormholeCount; i++) gw_pull(w->wormholes[i].pos, w->wormholes[i].mass, p, &ax, &ay);
    return vec2(ax, ay);
}

static void gw_bounce_asteroid_off(Asteroid *a, Vec2 c, float r) {
    float minDist = r + a->radius;
    if (vec2_dist_sq(a->pos, c) >= minDist * minDist) return;
    Vec2 n = vec2_normalized(vec2_sub(a->pos, c));
    a->pos = vec2_add(c, vec2_scale(n, minDist));
    float vn = vec2_dot(a->vel, n);
    if (vn < 0.0f) a->vel = vec2_sub(a->vel, vec2_scale(n, 2.0f * vn));
}

static void gw_update_asteroids(GameWorld *w, float dt) {
    for (int i = 0; i < w->asteroidCount; i++) {
        Asteroid *a = &w->asteroids[i];
        a->pos = vec2_add(a->pos, vec2_scale(a->vel, dt));
        // Arena walls
        if (a->pos.x < a->radius && a->vel.x < 0.0f) a->vel.x = -a->vel.x;
        if (a->pos.x > w->width - a->radius && a->vel.x > 0.0f) a->vel.x = -a->vel.x;
        if (a->pos.y < a->radius && a->vel.y < 0.0f) a->vel.y = -a->vel.y;
        if (a->pos.y > w->height - a->radius && a->vel.y > 0.0f) a->vel.y = -a->vel.y;
        // Bounce off suns, planets, wormholes
        for (int j = 0; j < w->starCount; j++) gw_bounce_asteroid_off(a, w->stars[j].pos, w->stars[j].radius);
        for (int j = 0; j < w->planetCount; j++) gw_bounce_asteroid_off(a, w->planets[j].pos, w->planets[j].radius);
        for (int j = 0; j < w->wormholeCount; j++) gw_bounce_asteroid_off(a, w->wormholes[j].pos, w->wormholes[j].horizon);
    }
    // Gentle asteroid-vs-asteroid separation (equal-mass elastic bounce)
    for (int i = 0; i < w->asteroidCount; i++) {
        for (int j = i + 1; j < w->asteroidCount; j++) {
            Asteroid *a = &w->asteroids[i];
            Asteroid *b = &w->asteroids[j];
            float minDist = a->radius + b->radius;
            if (vec2_dist_sq(a->pos, b->pos) >= minDist * minDist) continue;
            Vec2 n = vec2_normalized(vec2_sub(b->pos, a->pos));
            float overlap = minDist - vec2_dist(a->pos, b->pos);
            a->pos = vec2_sub(a->pos, vec2_scale(n, overlap / 2.0f));
            b->pos = vec2_add(b->pos, vec2_scale(n, overlap / 2.0f));
            float va = vec2_dot(a->vel, n);
            float vb = vec2_dot(b->vel, n);
            if (va - vb > 0.0f) {
                a->vel = vec2_add(a->vel, vec2_scale(n, vb - va));
                b->vel = vec2_add(b->vel, vec2_scale(n, va - vb));
            }
        }
    }
}

static void gw_steer_missile(GameWorld *w, Shot *shot, float dt) {
    Ship *target = NULL;
    float bestD2 = 0.0f;
    for (int i = 0; i < w->shipCount; i++) {
        Ship *s = &w->ships[i];
        if (!s->alive || s->id == shot->ownerId || s->cloakOn) continue;
        float d2 = vec2_dist_sq(s->pos, shot->pos);
        if (target == NULL || d2 < bestD2) { target = s; bestD2 = d2; }
    }
    if (target == NULL) return;
    if (vec2_dist(shot->pos, target->pos) > 2200.0f) return;
    float speed = maxf(vec2_length(shot->vel), 60.0f);
    float desired = vec2_angle(vec2_sub(target->pos, shot->pos));
    float current = vec2_angle(shot->vel);
    float turn = clampf(angle_diff(desired, current), -2.6f * dt, 2.6f * dt);
    shot->vel = vec2_from_angle_len(current + turn, speed);
}

static void gw_impact_damage(GameWorld *w, Ship *s, float impactSpeed) {
    if (impactSpeed > 120.0f) {
        ship_take_damage(s, w, (impactSpeed - 120.0f) * 0.06f, -1);
        gameworld_add_explosion(w, s->pos, 20.0f);
    }
}

static void gw_bounce_ship(GameWorld *w, Ship *s, Vec2 normal, Vec2 corrected) {
    s->pos = corrected;
    float vn = vec2_dot(s->vel, normal);
    if (vn < 0.0f) {
        s->vel = vec2_sub(s->vel, vec2_scale(normal, 1.6f * vn)); // restitution 0.6
        gw_impact_damage(w, s, -vn);
    }
}

static void gw_collide_ship_with_circle(GameWorld *w, Ship *s, Vec2 c, float r) {
    float minDist = r + s->radius;
    if (vec2_dist_sq(s->pos, c) >= minDist * minDist) return;
    Vec2 n = vec2_normalized(vec2_sub(s->pos, c));
    s->pos = vec2_add(c, vec2_scale(n, minDist));
    float vn = vec2_dot(s->vel, n);
    if (vn < 0.0f) {
        s->vel = vec2_sub(s->vel, vec2_scale(n, 1.6f * vn));
        gw_impact_damage(w, s, -vn);
    }
}

static void gw_collide_ship_with_world(GameWorld *w, Ship *s) {
    // Bounded arena walls: bounce, with damage on hard impacts.
    if (s->pos.x < s->radius) gw_bounce_ship(w, s, vec2(1.0f, 0.0f), vec2(s->radius, s->pos.y));
    if (s->pos.x > w->width - s->radius) gw_bounce_ship(w, s, vec2(-1.0f, 0.0f), vec2(w->width - s->radius, s->pos.y));
    if (s->pos.y < s->radius) gw_bounce_ship(w, s, vec2(0.0f, 1.0f), vec2(s->pos.x, s->radius));
    if (s->pos.y > w->height - s->radius) gw_bounce_ship(w, s, vec2(0.0f, -1.0f), vec2(s->pos.x, w->height - s->radius));

    for (int i = 0; i < w->asteroidCount; i++) gw_collide_ship_with_circle(w, s, w->asteroids[i].pos, w->asteroids[i].radius);
    if (!s->alive) return;

    for (int i = 0; i < w->starCount; i++) {
        if (vec2_dist(s->pos, w->stars[i].pos) < w->stars[i].radius + s->radius * 0.5f) {
            // Flying into a sun is not survivable.
            s->hull = 0.0f;
            s->alive = 0;
            gameworld_on_ship_destroyed(w, s, -1);
            return;
        }
    }

    // Brushing a planet captures the ship into a repair orbit.
    if (s->orbitCooldown <= 0.0f) {
        for (int i = 0; i < w->planetCount; i++) {
            Planet *p = &w->planets[i];
            if (vec2_dist(s->pos, p->pos) < p->radius + s->radius + 6.0f) {
                ship_enter_orbit(s, i, p->pos, p->vel, p->radius);
                return;
            }
        }
    }
}

/** Fall into a wormhole, get flung out of its twin. */
static void gw_ride_wormholes(GameWorld *w, Ship *s) {
    if (s->wormholeCooldown > 0.0f || w->wormholeCount < 2) return;
    for (int i = 0; i < w->wormholeCount; i++) {
        Wormhole *hole = &w->wormholes[i];
        if (vec2_dist(s->pos, hole->pos) >= hole->horizon + s->radius * 0.5f) continue;
        int exitCount = w->wormholeCount - 1;
        long pick = ((long)s->id + w->tick) % exitCount;
        if (pick < 0) pick += exitCount;
        int exitIdx = -1, seen = 0;
        for (int k = 0; k < w->wormholeCount; k++) {
            if (k == i) continue;
            if (seen == pick) { exitIdx = k; break; }
            seen++;
        }
        Wormhole *exit = &w->wormholes[exitIdx];
        gameworld_add_explosion(w, s->pos, 30.0f);
        Vec2 dir = vec2_normalized(s->vel);
        if (vec2_length(s->vel) < 20.0f) dir = vec2_normalized(vec2_sub(s->pos, hole->pos));
        s->pos = vec2_add(exit->pos, vec2_scale(dir, exit->horizon + s->radius + 40.0f));
        // Eject with at least escape-worthy speed so dead-drifters clear the
        // well before their gravity immunity runs out.
        if (vec2_length(s->vel) < MIN_WORMHOLE_EXIT_SPEED) s->vel = vec2_scale(dir, MIN_WORMHOLE_EXIT_SPEED);
        s->wormholeCooldown = WORMHOLE_GRACE;
        gameworld_add_explosion(w, s->pos, 30.0f);
        return;
    }
}

/** @return true if the shot died on scenery (or left through a wormhole). */
static int gw_collide_shot_with_world(GameWorld *w, Shot *shot) {
    Vec2 p = shot->pos;
    int hit = (p.x < 0.0f || p.x > w->width || p.y < 0.0f || p.y > w->height);
    for (int i = 0; i < w->asteroidCount && !hit; i++)
        if (vec2_dist(w->asteroids[i].pos, p) < w->asteroids[i].radius + shot->radius) hit = 1;
    for (int i = 0; i < w->planetCount && !hit; i++)
        if (vec2_dist(w->planets[i].pos, p) < w->planets[i].radius + shot->radius) hit = 1;
    for (int i = 0; i < w->starCount && !hit; i++)
        if (vec2_dist(w->stars[i].pos, p) < w->stars[i].radius + shot->radius) hit = 1;

    if (!hit) {
        for (int i = 0; i < w->wormholeCount; i++) {
            Wormhole *hole = &w->wormholes[i];
            if (vec2_dist(p, hole->pos) >= hole->horizon) continue;
            if (shot->kind == SHOT_MINE || w->wormholeCount < 2) {
                shot->alive = 0; // swallowed
                return 1;
            }
            // Shots ride wormholes too.
            int exitCount = w->wormholeCount - 1;
            long pick = ((long)shot->id + w->tick) % exitCount;
            if (pick < 0) pick += exitCount;
            int exitIdx = -1, seen = 0;
            for (int k = 0; k < w->wormholeCount; k++) {
                if (k == i) continue;
                if (seen == pick) { exitIdx = k; break; }
                seen++;
            }
            Wormhole *exit = &w->wormholes[exitIdx];
            Vec2 dir = (vec2_length(shot->vel) > 20.0f) ? vec2_normalized(shot->vel) : vec2_normalized(vec2_sub(shot->pos, hole->pos));
            shot->pos = vec2_add(exit->pos, vec2_scale(dir, exit->horizon + shot->radius + 30.0f));
            return 0;
        }
    }
    if (hit) {
        if (shot->kind == SHOT_MINE) {
            gw_detonate(w, shot);
        } else {
            gameworld_add_explosion(w, shot->pos, 18.0f);
        }
        shot->alive = 0;
        return 1;
    }
    return 0;
}

// ------------------------------------------------------------------
// Weapons
// ------------------------------------------------------------------

void gameworld_spawn_shot(GameWorld *w, Ship *owner, WeaponType wt, WeaponSpec spec) {
    if (w->shotCount >= MAX_SHOTS) return;
    Vec2 dir = vec2_from_angle(owner->heading);
    Shot *shot = &w->shots[w->shotCount++];
    if (wt == WEAPON_MINE) {
        Vec2 pos = vec2_sub(owner->pos, vec2_scale(dir, owner->radius + 22.0f));
        shot_init(shot, w->nextShotId++, SHOT_MINE, owner->id, pos, vec2_scale(owner->vel, 0.25f), spec.damage, spec.life);
    } else {
        ShotKind kind = (wt == WEAPON_PROJECTILE) ? SHOT_SLUG : SHOT_MISSILE;
        Vec2 pos = vec2_add(owner->pos, vec2_scale(dir, owner->radius + 8.0f));
        Vec2 vel = vec2_add(owner->vel, vec2_scale(dir, spec.speed));
        shot_init(shot, w->nextShotId++, kind, owner->id, pos, vel, spec.damage, spec.life);
    }
}

/**
 * Phaser: instantly locks every visible hostile in range and splits the
 * beam (and its damage) among them. @return false if nothing was in range.
 */
int gameworld_fire_beam(GameWorld *w, Ship *owner, WeaponSpec spec) {
    int targetIdx[MAX_SHIPS];
    int targetCount = 0;
    for (int i = 0; i < w->shipCount; i++) {
        Ship *t = &w->ships[i];
        if (t->alive && t->id != owner->id && !t->cloakOn && vec2_dist(t->pos, owner->pos) <= spec.range) {
            targetIdx[targetCount++] = i;
        }
    }
    if (targetCount == 0) return 0;
    float each = spec.damage / (float)targetCount;
    for (int k = 0; k < targetCount; k++) {
        Ship *t = &w->ships[targetIdx[k]];
        if (w->beamCount < MAX_BEAMS) {
            Beam *b = &w->beams[w->beamCount++];
            b->from = owner->pos;
            b->to = t->pos;
            b->age = 0.0f;
            b->alive = 1;
        }
        gameworld_add_explosion(w, t->pos, 22.0f);
        // Disruptor behavior: half the beam burns hull (shields can soak
        // it), half scrambles the target's energy banks directly.
        t->energy = maxf(t->energy - each * 0.5f, 0.0f);
        ship_take_damage(t, w, each * 0.5f, owner->id);
    }
    return 1;
}

static void gw_detonate(GameWorld *w, Shot *mine) {
    gameworld_add_explosion(w, mine->pos, 60.0f);
    for (int i = 0; i < w->shipCount; i++) {
        Ship *ship = &w->ships[i];
        if (!ship->alive) continue;
        float d = vec2_dist(ship->pos, mine->pos);
        if (d < MINE_BLAST_RADIUS + ship->radius) {
            float falloff = 1.0f - (d / (MINE_BLAST_RADIUS + ship->radius)) * 0.6f;
            ship_take_damage(ship, w, mine->damage * falloff, mine->ownerId);
            // Blast shove
            ship->vel = vec2_add(ship->vel, vec2_scale(vec2_normalized(vec2_sub(ship->pos, mine->pos)), 145.0f * falloff));
        }
    }
}

void gameworld_on_ship_destroyed(GameWorld *w, Ship *ship, int attackerId) {
    gameworld_add_explosion(w, ship->pos, 95.0f);
    if (attackerId >= 0) {
        for (int i = 0; i < w->shipCount; i++) {
            if (w->ships[i].id == attackerId) { w->ships[i].kills++; break; }
        }
    }
}

void gameworld_add_explosion(GameWorld *w, Vec2 pos, float size) {
    if (w->explosionCount >= MAX_EXPLOSIONS) return;
    Explosion *e = &w->explosions[w->explosionCount++];
    e->pos = pos;
    e->size = size;
    e->age = 0.0f;
    e->alive = 1;
}

static void gw_update_effects(GameWorld *w, float dt) {
    int n = 0;
    for (int i = 0; i < w->explosionCount; i++) {
        w->explosions[i].age += dt;
        if (w->explosions[i].age < EXPLOSION_DURATION) {
            if (n != i) w->explosions[n] = w->explosions[i];
            n++;
        }
    }
    w->explosionCount = n;

    n = 0;
    for (int i = 0; i < w->beamCount; i++) {
        w->beams[i].age += dt;
        if (w->beams[i].age < BEAM_DURATION) {
            if (n != i) w->beams[n] = w->beams[i];
            n++;
        }
    }
    w->beamCount = n;
}

static void gw_compact_shots(GameWorld *w) {
    int n = 0;
    for (int i = 0; i < w->shotCount; i++) {
        if (w->shots[i].alive) {
            if (n != i) w->shots[n] = w->shots[i];
            n++;
        }
    }
    w->shotCount = n;
}

static void gw_compact_ships(GameWorld *w) {
    int n = 0;
    for (int i = 0; i < w->shipCount; i++) {
        if (w->ships[i].alive) {
            if (n != i) w->ships[n] = w->ships[i];
            n++;
        }
    }
    w->shipCount = n;
}

void gameworld_step(GameWorld *w, float dt) {
    w->tick++;
    for (int i = 0; i < w->planetCount; i++) {
        Planet *p = &w->planets[i];
        planet_update(p, w->stars[p->starIndex].pos, dt);
    }
    gw_update_asteroids(w, dt);

    for (int i = 0; i < w->shipCount; i++) {
        Ship *s = &w->ships[i];
        if (!s->alive) continue;
        // Fresh wormhole transits and orbit departures get a grace period of
        // gravity immunity: both leave the ship deep inside a well (the
        // wormhole's own, or the sun the planet circles), and without it
        // slow ships are dragged straight back in.
        if (s->orbitPlanet < 0 && s->wormholeCooldown <= 0.0f && s->orbitCooldown <= 0.0f) {
            s->vel = vec2_add(s->vel, vec2_scale(gw_gravity_at(w, s->pos), dt));
        }
        ship_update(s, w, dt);
        if (s->orbitPlanet < 0) {
            gw_collide_ship_with_world(w, s);
            if (s->alive) gw_ride_wormholes(w, s);
        }
    }

    int newHitIdx[MAX_SHOTS];
    int newHitCount = 0;
    for (int i = 0; i < w->shotCount; i++) {
        Shot *shot = &w->shots[i];
        if (shot->kind != SHOT_MINE) shot->vel = vec2_add(shot->vel, vec2_scale(gw_gravity_at(w, shot->pos), dt));
        if (shot->kind == SHOT_MISSILE) gw_steer_missile(w, shot, dt);
        shot_update(shot, dt);

        if (shot->life <= 0.0f) {
            if (shot->kind == SHOT_MINE || shot->kind == SHOT_MISSILE) newHitIdx[newHitCount++] = i;
            shot->alive = 0;
            continue;
        }
        if (gw_collide_shot_with_world(w, shot)) continue;

        if (shot->kind == SHOT_MINE) {
            if (shot_armed(shot)) {
                int triggered = 0;
                for (int j = 0; j < w->shipCount; j++) {
                    Ship *ship = &w->ships[j];
                    if (ship->alive && ship->id != shot->ownerId && vec2_dist(ship->pos, shot->pos) < MINE_TRIGGER_RADIUS) {
                        triggered = 1;
                        break;
                    }
                }
                if (triggered) {
                    newHitIdx[newHitCount++] = i;
                    shot->alive = 0;
                }
            }
        } else {
            for (int j = 0; j < w->shipCount; j++) {
                Ship *ship = &w->ships[j];
                if (!ship->alive || ship->id == shot->ownerId) continue;
                if (vec2_dist(ship->pos, shot->pos) < ship->radius + shot->radius) {
                    ship_take_damage(ship, w, shot->damage, shot->ownerId);
                    gameworld_add_explosion(w, shot->pos, 28.0f);
                    shot->alive = 0;
                    break;
                }
            }
        }
    }
    for (int i = 0; i < newHitCount; i++) gw_detonate(w, &w->shots[newHitIdx[i]]);

    gw_compact_shots(w);
    gw_compact_ships(w);

    gw_update_effects(w, dt);
}
