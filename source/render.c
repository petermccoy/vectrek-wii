#include "render.h"
#include "gfx.h"
#include "palette.h"
#include "text.h"
#include <math.h>
#include <string.h>

static int visible(Vec2 p, float r, float left, float top, float right, float bottom) {
    return p.x + r > left && p.x - r < right && p.y + r > top && p.y - r < bottom;
}

static Vec2 w2s(const Camera *cam, Vec2 p) {
    return vec2((p.x - cam->camX) * cam->scale + cam->viewW / 2.0f,
                (p.y - cam->camY) * cam->scale + cam->viewH / 2.0f);
}

static void draw_starfield(const Camera *cam, float left, float top, float right, float bottom);
static void draw_bounds(const Camera *cam, GameWorld *world);
static void draw_star(const Camera *cam, Vec2 pos, float radius, float time);
static void draw_wormhole(const Camera *cam, Vec2 pos, float horizon, float time);
static void draw_asteroid(const Camera *cam, Asteroid *a);
static void draw_shot(const Camera *cam, Shot *s, float time);
static void draw_beam(const Camera *cam, Vec2 from, Vec2 to, float t);
static void draw_ship(const Camera *cam, Ship *ship, int isMe, float time);
static void draw_explosion(const Camera *cam, Vec2 pos, float size, float t);

void render_init(Camera *cam, float originX, float originY, float viewW, float viewH) {
    cam->originX = originX;
    cam->originY = originY;
    cam->viewW = viewW;
    cam->viewH = viewH;
    cam->scale = viewW / VIEW_SPAN;
    cam->camX = 4000.0f;
    cam->camY = 3000.0f;
}

Vec2 render_screen_to_world(const Camera *cam, Vec2 s) {
    return vec2((s.x - cam->viewW / 2.0f) / cam->scale + cam->camX,
                (s.y - cam->viewH / 2.0f) / cam->scale + cam->camY);
}

void render_draw_world(Camera *cam, GameWorld *world, Ship *me, float time) {
    cam->scale = cam->viewW / VIEW_SPAN;
    if (me != NULL) {
        cam->camX = clampf(me->pos.x, 0.0f, world->width);
        cam->camY = clampf(me->pos.y, 0.0f, world->height);
    }

    float halfW = cam->viewW / 2.0f / cam->scale;
    float halfH = cam->viewH / 2.0f / cam->scale;
    float left = cam->camX - halfW, right = cam->camX + halfW;
    float top = cam->camY - halfH, bottom = cam->camY + halfH;

    draw_starfield(cam, left, top, right, bottom);
    draw_bounds(cam, world);

    for (int i = 0; i < world->starCount; i++) {
        Star *star = &world->stars[i];
        Vec2 c = w2s(cam, star->pos);
        GXColor guide = pal_alpha(PAL_WHITE, 26);
        gfx_line_width(1.5f);
        for (int j = 0; j < world->planetCount; j++) {
            if (world->planets[j].starIndex == i) {
                gfx_circle_outline(c.x, c.y, world->planets[j].orbitRadius * cam->scale, 48, guide);
            }
        }
        if (visible(star->pos, star->radius * 2.4f, left, top, right, bottom)) {
            draw_star(cam, star->pos, star->radius, time);
        }
    }
    for (int i = 0; i < world->planetCount; i++) {
        Planet *p = &world->planets[i];
        if (!visible(p->pos, p->radius * 1.5f, left, top, right, bottom)) continue;
        Vec2 c = w2s(cam, p->pos);
        gfx_line_width(3.0f);
        gfx_circle_outline(c.x, c.y, p->radius * cam->scale, 24, PAL_PLANET);
        gfx_circle_outline(c.x, c.y, p->radius * 0.55f * cam->scale, 24, pal_alpha(PAL_PLANET, 70));
    }
    for (int i = 0; i < world->wormholeCount; i++) {
        Wormhole *h = &world->wormholes[i];
        if (visible(h->pos, h->horizon * 3.5f, left, top, right, bottom)) draw_wormhole(cam, h->pos, h->horizon, time);
    }
    for (int i = 0; i < world->asteroidCount; i++) {
        Asteroid *a = &world->asteroids[i];
        if (visible(a->pos, a->radius * 1.3f, left, top, right, bottom)) draw_asteroid(cam, a);
    }
    for (int i = 0; i < world->shotCount; i++) {
        Shot *s = &world->shots[i];
        if (visible(s->pos, 60.0f, left, top, right, bottom)) draw_shot(cam, s, time);
    }
    for (int i = 0; i < world->beamCount; i++) {
        Beam *b = &world->beams[i];
        draw_beam(cam, b->from, b->to, b->age / BEAM_DURATION);
    }
    for (int i = 0; i < world->shipCount; i++) {
        Ship *ship = &world->ships[i];
        if (!ship->alive) continue;
        // Cloaked hostiles are invisible.
        if (ship->cloakOn && ship != me) continue;
        if (visible(ship->pos, 90.0f, left, top, right, bottom)) draw_ship(cam, ship, ship == me, time);
    }
    for (int i = 0; i < world->explosionCount; i++) {
        Explosion *e = &world->explosions[i];
        draw_explosion(cam, e->pos, e->size, e->age / EXPLOSION_DURATION);
    }
}

// ------------------------------------------------------------------

static void draw_starfield(const Camera *cam, float left, float top, float right, float bottom) {
    const float cell = 420.0f;
    int ix = (int)floorf(left / cell) - 1;
    int ixEnd = (int)floorf(right / cell) + 1;
    int iyStart = (int)floorf(top / cell) - 1;
    int iyEnd = (int)floorf(bottom / cell) + 1;
    for (; ix <= ixEnd; ix++) {
        for (int iy = iyStart; iy <= iyEnd; iy++) {
            unsigned int h = ((unsigned int)(ix * 73856093)) ^ ((unsigned int)(iy * 19349663));
            for (int k = 0; k < 2; k++) {
                h = h * 1103515245u + 12345u;
                float fx = (float)((h >> 8) & 0x3FFu) / 1023.0f;
                float fy = (float)((h >> 18) & 0x3FFu) / 1023.0f;
                int alpha = 60 + (int)((h >> 4) & 0x7Fu);
                Vec2 sp = w2s(cam, vec2((ix + fx) * cell, (iy + fy) * cell));
                gfx_circle_fill(sp.x, sp.y, 2.2f * cam->scale, 6, pal_alpha(PAL_WHITE, alpha));
            }
        }
    }
}

static void draw_bounds(const Camera *cam, GameWorld *world) {
    Vec2 a = w2s(cam, vec2(0.0f, 0.0f));
    Vec2 b = w2s(cam, vec2(world->width, world->height));
    gfx_line_width(4.0f);
    gfx_rect_outline(a.x, a.y, b.x, b.y, pal_alpha(PAL_BOUNDS, 160));
    Vec2 a2 = w2s(cam, vec2(-14.0f, -14.0f));
    Vec2 b2 = w2s(cam, vec2(world->width + 14.0f, world->height + 14.0f));
    gfx_rect_outline(a2.x, a2.y, b2.x, b2.y, pal_alpha(PAL_BOUNDS, 60));
}

static void draw_star(const Camera *cam, Vec2 pos, float radius, float time) {
    float pulse = radius * (1.0f + 0.03f * sinf(time * 2.4f));
    Vec2 c = w2s(cam, pos);
    float pr = pulse * cam->scale;
    gfx_line_width(4.0f);
    gfx_circle_outline(c.x, c.y, pr, 28, PAL_STAR);
    gfx_circle_outline(c.x, c.y, pr * 1.28f, 28, pal_alpha(PAL_STAR, 90));
    gfx_line_width(2.5f);
    const int rays = 9;
    GXColor rc = pal_alpha(PAL_STAR, 200);
    for (int i = 0; i < rays; i++) {
        float a = time * 0.15f + i * TWO_PI / rays;
        float cx = cosf(a), sy = sinf(a);
        float innerR = pr * 1.4f;
        float outerR = pr * (1.75f + 0.1f * sinf(time * 3.0f + i));
        gfx_line(c.x + cx * innerR, c.y + sy * innerR, c.x + cx * outerR, c.y + sy * outerR, rc);
    }
}

static void draw_wormhole(const Camera *cam, Vec2 pos, float horizon, float time) {
    Vec2 c = w2s(cam, pos);
    float hr = horizon * cam->scale;
    gfx_circle_fill(c.x, c.y, hr, 24, PAL_BLACK);
    gfx_line_width(3.0f);
    gfx_circle_outline(c.x, c.y, hr, 24, PAL_HOLE);
    // Counter-rotating swirl arcs: a portal, not a grave.
    gfx_line_width(2.5f);
    for (int ring = 1; ring <= 3; ring++) {
        float r = hr * (1.0f + ring * 0.55f);
        GXColor col = pal_alpha((ring % 2 == 0) ? PAL_HOLE : PAL_SELF, 170 - ring * 40);
        float dir = (ring % 2 == 0) ? 1.0f : -1.0f;
        float startDeg = dir * time * 110.0f + ring * 100.0f;
        float startRad = startDeg * (PIF / 180.0f);
        float sweepRad = 210.0f * (PIF / 180.0f);
        gfx_arc_outline(c.x, c.y, r, startRad, sweepRad, 24, col);
    }
    int a = (int)clampf(110.0f + 60.0f * sinf(time * 5.0f), 50.0f, 200.0f);
    gfx_circle_fill(c.x, c.y, hr * 0.28f, 16, pal_alpha(PAL_SELF, a));
}

static void draw_asteroid(const Camera *cam, Asteroid *a) {
    Vec2 pts[ASTEROID_SHAPE_VERTS];
    for (int i = 0; i < ASTEROID_SHAPE_VERTS; i++) {
        float ang = (float)i / ASTEROID_SHAPE_VERTS * TWO_PI;
        float r = a->radius * a->shape[i];
        pts[i] = w2s(cam, vec2(a->pos.x + cosf(ang) * r, a->pos.y + sinf(ang) * r));
    }
    gfx_line_width(3.0f);
    gfx_polyline(pts, ASTEROID_SHAPE_VERTS, 1, PAL_ASTEROID);
}

/** Rotate a ship-local offset by heading and project to screen. */
static Vec2 ship_pt(const Camera *cam, Vec2 origin, float ca, float sa, float lx, float ly) {
    float rx = lx * ca - ly * sa;
    float ry = lx * sa + ly * ca;
    return w2s(cam, vec2(origin.x + rx, origin.y + ry));
}

static void draw_shot(const Camera *cam, Shot *s, float time) {
    switch (s->kind) {
        case SHOT_SLUG: {
            Vec2 d = vec2_from_angle_len(s->heading, 12.0f);
            Vec2 p1 = w2s(cam, vec2(s->pos.x - d.x, s->pos.y - d.y));
            Vec2 p2 = w2s(cam, vec2(s->pos.x + d.x, s->pos.y + d.y));
            gfx_line_width(3.0f);
            gfx_line(p1.x, p1.y, p2.x, p2.y, PAL_SLUG);
            break;
        }
        case SHOT_MISSILE: {
            float ca = cosf(s->heading), sa = sinf(s->heading);
            Vec2 pts[3] = {
                ship_pt(cam, s->pos, ca, sa, 12.0f, 0.0f),
                ship_pt(cam, s->pos, ca, sa, -8.0f, 6.0f),
                ship_pt(cam, s->pos, ca, sa, -8.0f, -6.0f),
            };
            gfx_line_width(3.0f);
            gfx_polyline(pts, 3, 1, PAL_MISSILE);
            Vec2 fa = ship_pt(cam, s->pos, ca, sa, -8.0f, 0.0f);
            Vec2 fb = ship_pt(cam, s->pos, ca, sa, -16.0f - 6.0f * sinf(time * 40.0f), 0.0f);
            gfx_line(fa.x, fa.y, fb.x, fb.y, pal_alpha(PAL_MISSILE, 170));
            break;
        }
        case SHOT_MINE: {
            int blink = !shot_armed(s) || sinf(time * 9.0f) > -0.3f;
            GXColor col = pal_alpha(PAL_MINE, blink ? 255 : 90);
            Vec2 c = w2s(cam, s->pos);
            gfx_line_width(3.0f);
            gfx_circle_outline(c.x, c.y, 9.0f * cam->scale, 16, col);
            for (int i = 0; i < 4; i++) {
                float a = i * TWO_PI / 4.0f + 0.6f;
                Vec2 p1 = w2s(cam, vec2(s->pos.x + cosf(a) * 9.0f, s->pos.y + sinf(a) * 9.0f));
                Vec2 p2 = w2s(cam, vec2(s->pos.x + cosf(a) * 16.0f, s->pos.y + sinf(a) * 16.0f));
                gfx_line(p1.x, p1.y, p2.x, p2.y, col);
            }
            break;
        }
    }
}

static void draw_beam(const Camera *cam, Vec2 from, Vec2 to, float t) {
    int alpha = (int)clampf((1.0f - t) * 255.0f, 0.0f, 255.0f);
    Vec2 a = w2s(cam, from), b = w2s(cam, to);
    gfx_line_width(5.0f);
    gfx_line(a.x, a.y, b.x, b.y, pal_alpha(PAL_BOLT, alpha));
    gfx_line_width(12.0f);
    gfx_line(a.x, a.y, b.x, b.y, pal_alpha(PAL_BOLT, alpha / 4));
    gfx_circle_fill(b.x, b.y, (8.0f * (1.0f - t) + 2.0f) * cam->scale, 12, pal_alpha(PAL_BOLT, alpha));
}

static void draw_ship(const Camera *cam, Ship *ship, int isMe, float time) {
    GXColor color = isMe ? PAL_SELF : PAL_ENEMY;
    int alpha = ship->cloakOn ? 70 : 255;
    float r = ship->radius;
    Vec2 c = w2s(cam, ship->pos);
    float ca = cosf(ship->heading), sa = sinf(ship->heading);
    GXColor hullColor = pal_alpha(color, alpha);

    gfx_line_width(3.0f);
    switch (ship->hullStyle) {
        case 1: { // CRUISER: long hull with swept wings, Netrek-flavored.
            Vec2 pts[8] = {
                ship_pt(cam, ship->pos, ca, sa, r * 1.3f, 0.0f),
                ship_pt(cam, ship->pos, ca, sa, r * 0.2f, r * 0.35f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.7f, r * 0.95f),
                ship_pt(cam, ship->pos, ca, sa, -r * 1.0f, r * 0.55f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.6f, 0.0f),
                ship_pt(cam, ship->pos, ca, sa, -r * 1.0f, -r * 0.55f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.7f, -r * 0.95f),
                ship_pt(cam, ship->pos, ca, sa, r * 0.2f, -r * 0.35f),
            };
            gfx_polyline(pts, 8, 1, hullColor);
            Vec2 dot = ship_pt(cam, ship->pos, ca, sa, r * 0.55f, 0.0f);
            gfx_circle_fill(dot.x, dot.y, r * 0.18f * cam->scale, 10, pal_alpha(color, (int)(alpha * 0.7f)));
            break;
        }
        case 2: { // TALON: forked twin-prong fighter, Omega Race-flavored.
            Vec2 pts[6] = {
                ship_pt(cam, ship->pos, ca, sa, r * 1.1f, r * 0.45f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.9f, r * 0.7f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.5f, 0.0f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.9f, -r * 0.7f),
                ship_pt(cam, ship->pos, ca, sa, r * 1.1f, -r * 0.45f),
                ship_pt(cam, ship->pos, ca, sa, r * 0.3f, 0.0f),
            };
            gfx_polyline(pts, 6, 1, hullColor);
            Vec2 s1 = ship_pt(cam, ship->pos, ca, sa, -r * 0.5f, 0.0f);
            Vec2 s2 = ship_pt(cam, ship->pos, ca, sa, r * 0.3f, 0.0f);
            gfx_line(s1.x, s1.y, s2.x, s2.y, pal_alpha(color, (int)(alpha * 0.7f)));
            break;
        }
        default: { // SABER: angular dart with a canopy line.
            Vec2 pts[5] = {
                ship_pt(cam, ship->pos, ca, sa, r * 1.2f, 0.0f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.9f, r * 0.8f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.4f, r * 0.3f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.4f, -r * 0.3f),
                ship_pt(cam, ship->pos, ca, sa, -r * 0.9f, -r * 0.8f),
            };
            gfx_polyline(pts, 5, 1, hullColor);
            Vec2 s1 = ship_pt(cam, ship->pos, ca, sa, r * 0.5f, 0.0f);
            Vec2 s2 = ship_pt(cam, ship->pos, ca, sa, -r * 0.3f, 0.0f);
            gfx_line(s1.x, s1.y, s2.x, s2.y, pal_alpha(color, (int)(alpha * 0.7f)));
            break;
        }
    }

    if (ship->thrusting) {
        float flick = 0.7f + 0.3f * sinf(time * 47.0f + ship->id * 3.0f);
        GXColor fc = pal_alpha(PAL_MISSILE, (int)(alpha * 0.85f));
        gfx_line_width(3.0f);
        Vec2 f1a = ship_pt(cam, ship->pos, ca, sa, -r * 0.6f, r * 0.3f);
        Vec2 f1b = ship_pt(cam, ship->pos, ca, sa, -r * (1.1f + flick), 0.0f);
        Vec2 f2a = ship_pt(cam, ship->pos, ca, sa, -r * 0.6f, -r * 0.3f);
        gfx_line(f1a.x, f1a.y, f1b.x, f1b.y, fc);
        gfx_line(f2a.x, f2a.y, f1b.x, f1b.y, fc);
    }

    if (ship->shieldOn) {
        gfx_line_width(2.5f);
        int a2 = (int)clampf(90.0f + 50.0f * sinf(time * 6.0f + ship->id), 40.0f, 160.0f);
        gfx_circle_outline(c.x, c.y, r * 1.7f * cam->scale, 24, pal_alpha(PAL_SHIELD, a2));
    }
    if (ship_in_orbit(ship)) {
        // Parked halo: repairing at anchor.
        gfx_line_width(2.0f);
        gfx_circle_outline(c.x, c.y, r * 2.1f * cam->scale, 32, pal_alpha(PAL_PLANET, 120));
    }

    if (!isMe && !ship->cloakOn) {
        Vec2 sp = w2s(cam, vec2(ship->pos.x, ship->pos.y - r * 2.2f));
        int col = (int)((cam->originX + sp.x) / 8.0f) - (int)(strlen(ship->name) / 2);
        int row = (int)((cam->originY + sp.y) / 16.0f);
        if (col < 0) col = 0;
        if (row < 0) row = 0;
        text_queue(row, col, TXT_CYAN, "%s", ship->name);
    }
}

static void draw_explosion(const Camera *cam, Vec2 pos, float size, float t) {
    int alpha = (int)clampf((1.0f - t) * 255.0f, 0.0f, 255.0f);
    Vec2 c = w2s(cam, pos);
    gfx_line_width(3.0f);
    gfx_circle_outline(c.x, c.y, size * (0.3f + t * 2.0f) * cam->scale, 20, pal_alpha(PAL_BOOM, alpha));
    gfx_line_width(2.5f);
    for (int i = 0; i < 8; i++) {
        float a = i * TWO_PI / 8.0f + pos.x; // pos-derived phase so booms differ
        float inner = size * t * 1.4f;
        float outer = size * (0.4f + t * 2.4f);
        Vec2 p1 = w2s(cam, vec2(pos.x + cosf(a) * inner, pos.y + sinf(a) * inner));
        Vec2 p2 = w2s(cam, vec2(pos.x + cosf(a) * outer, pos.y + sinf(a) * outer));
        gfx_line(p1.x, p1.y, p2.x, p2.y, pal_alpha(PAL_BOOM, alpha));
    }
}
