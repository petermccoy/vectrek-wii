// Minimal GX 2D immediate-mode drawing helpers: lines, polylines, circles,
// arcs and filled shapes in screen pixel space (the projection matrix is set
// up once in main.c as an orthographic view matching the framebuffer).
#ifndef VECTREK_GFX_H
#define VECTREK_GFX_H

#include <gccore.h>
#include "vec2.h"

static inline GXColor rgba(u8 r, u8 g, u8 b, u8 a) { GXColor c = { r, g, b, a }; return c; }

void gfx_line_width(float pixels);

void gfx_line(float x1, float y1, float x2, float y2, GXColor c);
void gfx_polyline(const Vec2 *pts, int n, int closed, GXColor c);

void gfx_circle_outline(float cx, float cy, float r, int segments, GXColor c);
void gfx_circle_fill(float cx, float cy, float r, int segments, GXColor c);
/** Angles in radians, sweeping from startAngle by sweepAngle (can be negative). */
void gfx_arc_outline(float cx, float cy, float r, float startAngle, float sweepAngle, int segments, GXColor c);

void gfx_rect_outline(float x0, float y0, float x1, float y1, GXColor c);
void gfx_rect_fill(float x0, float y0, float x1, float y1, GXColor c);

#endif
