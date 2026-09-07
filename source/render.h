// Vector-style world rendering. Ported from Renderer.kt. Also owns the
// screen<->world transform used for aiming.
#ifndef VECTREK_RENDER_H
#define VECTREK_RENDER_H

#include "gameworld.h"
#include "entity.h"

typedef struct {
    float viewW, viewH;
    float scale;
    float camX, camY;
} Camera;

#define VIEW_SPAN 1500.0f  // world units visible across the screen width

void render_init(Camera *cam, float viewW, float viewH);
Vec2 render_screen_to_world(const Camera *cam, Vec2 screenPt);

/** me may be NULL (no local ship to lock the camera to yet). */
void render_draw_world(Camera *cam, GameWorld *world, Ship *me, float time);

#endif
