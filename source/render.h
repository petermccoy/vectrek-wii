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
    // Pixel offset of this viewport's top-left corner within the full
    // framebuffer. GX draws stay viewport-local (the GX viewport transform
    // itself places them on screen), but console text is addressed in
    // global framebuffer character-grid coordinates, so text placement
    // needs this to land in the right split-screen quadrant.
    float originX, originY;
} Camera;

#define VIEW_SPAN 1500.0f  // world units visible across the screen width

/** originX/originY are this viewport's pixel offset within the full
 *  framebuffer (0,0 for a full-screen single-player view). */
void render_init(Camera *cam, float originX, float originY, float viewW, float viewH);
Vec2 render_screen_to_world(const Camera *cam, Vec2 screenPt);

/** me may be NULL (no local ship to lock the camera to yet). */
void render_draw_world(Camera *cam, GameWorld *world, Ship *me, float time);

#endif
