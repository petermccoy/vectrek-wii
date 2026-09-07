#include "gfx.h"
#include <math.h>

void gfx_line_width(float pixels) {
    int w = (int)(pixels * 6.0f);
    if (w < 6) w = 6;   // GX minimum ~1px (6 = 1.0 in 1/6-pixel units)
    if (w > 255) w = 255;
    GX_SetLineWidth((u8)w, GX_TO_ZERO);
}

void gfx_line(float x1, float y1, float x2, float y2, GXColor c) {
    GX_Begin(GX_LINES, GX_VTXFMT0, 2);
    GX_Position2f32(x1, y1); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_Position2f32(x2, y2); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_End();
}

void gfx_polyline(const Vec2 *pts, int n, int closed, GXColor c) {
    if (n < 2) return;
    int count = n + (closed ? 1 : 0);
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, count);
    for (int i = 0; i < n; i++) {
        GX_Position2f32(pts[i].x, pts[i].y);
        GX_Color4u8(c.r, c.g, c.b, c.a);
    }
    if (closed) {
        GX_Position2f32(pts[0].x, pts[0].y);
        GX_Color4u8(c.r, c.g, c.b, c.a);
    }
    GX_End();
}

void gfx_circle_outline(float cx, float cy, float r, int segments, GXColor c) {
    if (segments < 3) segments = 3;
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, segments + 1);
    for (int i = 0; i <= segments; i++) {
        float a = (float)i / (float)segments * TWO_PI;
        GX_Position2f32(cx + cosf(a) * r, cy + sinf(a) * r);
        GX_Color4u8(c.r, c.g, c.b, c.a);
    }
    GX_End();
}

void gfx_circle_fill(float cx, float cy, float r, int segments, GXColor c) {
    if (segments < 3) segments = 3;
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, segments + 2);
    GX_Position2f32(cx, cy); GX_Color4u8(c.r, c.g, c.b, c.a);
    for (int i = 0; i <= segments; i++) {
        float a = (float)i / (float)segments * TWO_PI;
        GX_Position2f32(cx + cosf(a) * r, cy + sinf(a) * r);
        GX_Color4u8(c.r, c.g, c.b, c.a);
    }
    GX_End();
}

void gfx_arc_outline(float cx, float cy, float r, float startAngle, float sweepAngle, int segments, GXColor c) {
    if (segments < 1) segments = 1;
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, segments + 1);
    for (int i = 0; i <= segments; i++) {
        float a = startAngle + sweepAngle * ((float)i / (float)segments);
        GX_Position2f32(cx + cosf(a) * r, cy + sinf(a) * r);
        GX_Color4u8(c.r, c.g, c.b, c.a);
    }
    GX_End();
}

void gfx_rect_outline(float x0, float y0, float x1, float y1, GXColor c) {
    Vec2 pts[4] = { vec2(x0, y0), vec2(x1, y0), vec2(x1, y1), vec2(x0, y1) };
    gfx_polyline(pts, 4, 1, c);
}

void gfx_rect_fill(float x0, float y0, float x1, float y1, GXColor c) {
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, 4);
    GX_Position2f32(x0, y0); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_Position2f32(x1, y0); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_Position2f32(x1, y1); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_Position2f32(x0, y1); GX_Color4u8(c.r, c.g, c.b, c.a);
    GX_End();
}
