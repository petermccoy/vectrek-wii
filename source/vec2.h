// 2D vector math. Ported from Vec2.kt.
#ifndef VECTREK_VEC2_H
#define VECTREK_VEC2_H

#include <math.h>

typedef struct {
    float x, y;
} Vec2;

#define PIF 3.14159265358979323846f
#define TWO_PI (2.0f * PIF)

static inline Vec2 vec2(float x, float y) { Vec2 v = { x, y }; return v; }
static inline Vec2 vec2_zero(void) { return vec2(0.0f, 0.0f); }

static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return vec2(a.x + b.x, a.y + b.y); }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return vec2(a.x - b.x, a.y - b.y); }
static inline Vec2 vec2_scale(Vec2 a, float s) { return vec2(a.x * s, a.y * s); }
static inline Vec2 vec2_div(Vec2 a, float s) { return vec2(a.x / s, a.y / s); }

static inline float vec2_length_sq(Vec2 a) { return a.x * a.x + a.y * a.y; }
static inline float vec2_length(Vec2 a) { return sqrtf(vec2_length_sq(a)); }
static inline float vec2_dist_sq(Vec2 a, Vec2 b) { return vec2_length_sq(vec2_sub(a, b)); }
static inline float vec2_dist(Vec2 a, Vec2 b) { return vec2_length(vec2_sub(a, b)); }
static inline float vec2_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static inline float vec2_angle(Vec2 a) { return atan2f(a.y, a.x); }

static inline Vec2 vec2_normalized(Vec2 a) {
    float l = vec2_length(a);
    if (l < 1e-6f) return vec2_zero();
    return vec2(a.x / l, a.y / l);
}

static inline Vec2 vec2_from_angle_len(float a, float len) {
    return vec2(cosf(a) * len, sinf(a) * len);
}
static inline Vec2 vec2_from_angle(float a) { return vec2_from_angle_len(a, 1.0f); }

/** Smallest signed angle taking you from b to a, in (-pi, pi]. */
static inline float angle_diff(float a, float b) {
    float d = a - b;
    while (d > PIF) d -= TWO_PI;
    while (d < -PIF) d += TWO_PI;
    return d;
}

static inline float wrap_angle(float a) {
    float r = a;
    while (r > PIF) r -= TWO_PI;
    while (r < -PIF) r += TWO_PI;
    return r;
}

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }

#endif
