// Old-school vector look: bright strokes on black. Ported from Renderer.kt's
// Palette object (Android ARGB ints converted to GX RGBA8).
#ifndef VECTREK_PALETTE_H
#define VECTREK_PALETTE_H

#include <gccore.h>

extern const GXColor PAL_SELF;
extern const GXColor PAL_ENEMY;
extern const GXColor PAL_ASTEROID;
extern const GXColor PAL_STAR;
extern const GXColor PAL_PLANET;
extern const GXColor PAL_HOLE;
extern const GXColor PAL_SHIELD;
extern const GXColor PAL_BOLT;
extern const GXColor PAL_SLUG;
extern const GXColor PAL_MISSILE;
extern const GXColor PAL_MINE;
extern const GXColor PAL_BOOM;
extern const GXColor PAL_BOUNDS;
extern const GXColor PAL_TEXT_DIM;
extern const GXColor PAL_WHITE;
extern const GXColor PAL_BLACK;

/** Same color with alpha replaced (0-255, clamped). */
GXColor pal_alpha(GXColor c, int alpha255);

#endif
