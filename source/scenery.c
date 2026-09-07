#include "scenery.h"

void planet_update(Planet *p, Vec2 starPos, float dt) {
    p->orbitAngle = wrap_angle(p->orbitAngle + p->orbitSpeed * dt);
    Vec2 next = vec2(starPos.x + cosf(p->orbitAngle) * p->orbitRadius,
                      starPos.y + sinf(p->orbitAngle) * p->orbitRadius);
    p->vel = (dt > 0.0f) ? vec2_scale(vec2_sub(next, p->pos), 1.0f / dt) : vec2_zero();
    p->pos = next;
}
