#include "hud.h"
#include "gfx.h"
#include "palette.h"
#include "text.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static float dp(float v, float scale) { return v * scale; }

void hud_init(Hud *hud) {
    memset(hud, 0, sizeof(*hud));
    hud->laidOutW = -1;
    hud->laidOutH = -1;
}

void hud_layout(Hud *hud, int w, int h, const Loadout *lo) {
    if (w == hud->laidOutW && h == hud->laidOutH && hud->buttonCount > 0) return;
    hud->laidOutW = w;
    hud->laidOutH = h;
    hud->buttonCount = 0;
    float scale = (float)h / 480.0f;
    float r = dp(28.0f, scale);
    float gap = dp(70.0f, scale);
    float rowY = dp(104.0f, scale);

    static const char *defenseKeys[DEFENSE_COUNT] = { "-", "+" };
    static const char *weaponKeys[4] = { "A", "1", "2", "Z" };

    int di = 0;
    for (int d = 0; d < DEFENSE_COUNT; d++) {
        if (!lo->defenseEquipped[d]) continue;
        HudBtn *b = &hud->buttons[hud->buttonCount++];
        memset(b, 0, sizeof(*b));
        b->hasDefense = 1;
        b->defense = (DefenseType)d;
        b->cx = dp(50.0f, scale) + di * gap;
        b->cy = rowY;
        b->r = r;
        strncpy(b->keyLabel, defenseKeys[d], sizeof(b->keyLabel) - 1);
        di++;
    }
    int wi = 0;
    for (int wt = 0; wt < WEAPON_COUNT; wt++) {
        if (!lo->weaponEquipped[wt]) continue;
        HudBtn *b = &hud->buttons[hud->buttonCount++];
        memset(b, 0, sizeof(*b));
        b->hasWeapon = 1;
        b->weapon = (WeaponType)wt;
        b->cx = (float)w - dp(180.0f, scale) - wi * gap;
        b->cy = rowY;
        b->r = r;
        strncpy(b->keyLabel, weaponKeys[wi < 4 ? wi : 3], sizeof(b->keyLabel) - 1);
        wi++;
    }
}

static void draw_bars(const Camera *cam, Ship *me, float scale) {
    float x0 = dp(60.0f, scale);
    float y0 = dp(20.0f, scale);
    float bw = dp(150.0f, scale);
    float bh = dp(9.0f, scale);
    float rowStep = bh + dp(8.0f, scale);
    int labelCol = (int)(cam->originX / 8.0f);

    float hullFrac = clampf(me->hull / me->maxHull, 0.0f, 1.0f);
    GXColor hullColor = rgba(
        (u8)clampf(255.0f * (1.0f - hullFrac * 0.7f), 0.0f, 255.0f),
        (u8)clampf(220.0f * hullFrac + 40.0f, 0.0f, 255.0f),
        70, 255);
    gfx_rect_fill(x0, y0, x0 + bw * hullFrac, y0 + bh, hullColor);
    gfx_line_width(1.2f);
    gfx_rect_outline(x0, y0, x0 + bw, y0 + bh, pal_alpha(hullColor, 160));
    text_queue((int)((cam->originY + y0) / 16.0f), labelCol, TXT_WHITE, "HULL");

    float energyFrac = clampf(me->energy / me->maxEnergy, 0.0f, 1.0f);
    float y1 = y0 + rowStep;
    gfx_rect_fill(x0, y1, x0 + bw * energyFrac, y1 + bh, PAL_SELF);
    gfx_line_width(1.2f);
    gfx_rect_outline(x0, y1, x0 + bw, y1 + bh, pal_alpha(PAL_SELF, 160));
    text_queue((int)((cam->originY + y1) / 16.0f), labelCol, TXT_CYAN, "ENRG");
}

static void draw_radar(const Camera *cam, GameWorld *world, Ship *me, float time, float scale) {
    float r = dp(64.0f, scale);
    float cx = cam->viewW - r - dp(18.0f, scale);
    float cy = r + dp(18.0f, scale);
    float range = me->stats.radarRange;
    float k = r / range;

    gfx_line_width(1.4f);
    gfx_circle_outline(cx, cy, r, 32, pal_alpha(PAL_SELF, 120));
    gfx_circle_outline(cx, cy, r * 0.5f, 32, pal_alpha(PAL_SELF, 45));
    float sweep = time * 1.5f;
    gfx_line(cx, cy, cx + cosf(sweep) * r, cy + sinf(sweep) * r, pal_alpha(PAL_SELF, 120));

#define RADAR_BLIP(px, py, color, size) do { \
        float dx = ((px) - me->pos.x) * k, dy = ((py) - me->pos.y) * k; \
        if (dx * dx + dy * dy <= r * r) gfx_circle_fill(cx + dx, cy + dy, (size), 8, pal_alpha((color), 230)); \
    } while (0)

    for (int i = 0; i < world->asteroidCount; i++) RADAR_BLIP(world->asteroids[i].pos.x, world->asteroids[i].pos.y, PAL_ASTEROID, dp(1.5f, scale));
    for (int i = 0; i < world->starCount; i++) RADAR_BLIP(world->stars[i].pos.x, world->stars[i].pos.y, PAL_STAR, dp(3.0f, scale));
    for (int i = 0; i < world->planetCount; i++) RADAR_BLIP(world->planets[i].pos.x, world->planets[i].pos.y, PAL_PLANET, dp(2.0f, scale));
    for (int i = 0; i < world->wormholeCount; i++) RADAR_BLIP(world->wormholes[i].pos.x, world->wormholes[i].pos.y, PAL_HOLE, dp(3.0f, scale));
    for (int i = 0; i < world->shotCount; i++) {
        Shot *s = &world->shots[i];
        if (s->kind == SHOT_MINE) {
            if (s->ownerId == me->id) RADAR_BLIP(s->pos.x, s->pos.y, PAL_MINE, dp(1.5f, scale));
        } else if (s->kind == SHOT_MISSILE) {
            RADAR_BLIP(s->pos.x, s->pos.y, PAL_MISSILE, dp(1.5f, scale));
        }
    }
    for (int i = 0; i < world->shipCount; i++) {
        Ship *sh = &world->ships[i];
        if (sh->id == me->id || !sh->alive || sh->cloakOn) continue;
        RADAR_BLIP(sh->pos.x, sh->pos.y, PAL_ENEMY, dp(2.5f, scale));
    }
#undef RADAR_BLIP

    gfx_circle_fill(cx, cy, dp(2.0f, scale), 8, PAL_SELF);
}

static void draw_buttons(const Camera *cam, Hud *hud, GameWorld *world, Ship *me, const ShipInput *input, float time) {
    for (int bi = 0; bi < hud->buttonCount; bi++) {
        HudBtn *b = &hud->buttons[bi];
        GXColor color = PAL_SELF;
        int active = 0, usable = 1, locked = 0;
        char sub[16] = "";

        if (b->hasWeapon) {
            WeaponType wt = b->weapon;
            int lvl = loadout_weapon_level(&me->loadout, wt);
            WeaponSpec spec = weapon_spec(wt, lvl);
            if (spec.ammo >= 0) {
                int left = me->ammo[wt];
                snprintf(sub, sizeof(sub), "x%d", left);
                if (left <= 0) { color = PAL_ENEMY; usable = 0; }
            } else {
                snprintf(sub, sizeof(sub), "%de", (int)spec.energyCost);
                if (me->energy < spec.energyCost) usable = 0;
                for (int i = 0; i < world->shipCount; i++) {
                    Ship *o = &world->ships[i];
                    if (o->alive && o->id != me->id && !o->cloakOn && vec2_dist(o->pos, me->pos) <= spec.range) {
                        locked = 1;
                        break;
                    }
                }
                if (!locked) usable = 0;
            }
            active = input->fireHeld[wt];
        }
        if (b->hasDefense) {
            color = PAL_SHIELD;
            active = (b->defense == DEFENSE_SHIELD) ? input->shield : input->cloak;
            if (me->energy < 2.0f) usable = 0;
        }

        if (active || locked) {
            GXColor fillc = (locked && !active) ? PAL_BOLT : color;
            int a = (locked && !active) ? (int)clampf(40.0f + 25.0f * sinf(time * 8.0f), 20.0f, 70.0f) : 60;
            gfx_circle_fill(b->cx, b->cy, b->r, 20, pal_alpha(fillc, a));
        }
        GXColor strokeC = locked ? PAL_BOLT : color;
        gfx_line_width(1.8f);
        gfx_circle_outline(b->cx, b->cy, b->r, 24, pal_alpha(strokeC, usable ? 220 : 80));

        if (b->hasWeapon) {
            int lvl = loadout_weapon_level(&me->loadout, b->weapon);
            WeaponSpec spec = weapon_spec(b->weapon, lvl);
            float cd = ship_cooldown(me, b->weapon);
            if (cd > 0.0f && spec.cooldown > 0.0f) {
                gfx_line_width(2.5f);
                float sweepAmt = TWO_PI * (cd / spec.cooldown);
                gfx_arc_outline(b->cx, b->cy, b->r, -PIF / 2.0f, sweepAmt, 24, pal_alpha(strokeC, 255));
            }
        }

        int col = (int)((cam->originX + b->cx) / 8.0f) - 3;
        int row = (int)((cam->originY + b->cy) / 16.0f) + 2;
        if (col < 0) col = 0;
        const char *label = b->hasWeapon ? WEAPON_INFO[b->weapon].shortLabel : DEFENSE_INFO[b->defense].shortLabel;
        text_queue(row, col, usable ? TXT_WHITE : TXT_YELLOW, "%s[%s]", label, b->keyLabel);
        if (sub[0]) text_queue(row + 1, col, usable ? TXT_WHITE : TXT_YELLOW, "%s", sub);
    }
}

static void draw_leave_orbit(const Camera *cam, float time, float scale) {
    float cx = cam->viewW / 2.0f, cy = cam->viewH - dp(64.0f, scale), r = dp(34.0f, scale);
    int a = (int)clampf(30.0f + 20.0f * sinf(time * 4.0f), 15.0f, 55.0f);
    gfx_circle_fill(cx, cy, r, 24, pal_alpha(PAL_PLANET, a));
    gfx_line_width(2.0f);
    gfx_circle_outline(cx, cy, r, 24, pal_alpha(PAL_PLANET, 230));
    int col = (int)((cam->originX + cx) / 8.0f) - 6;
    int row = (int)((cam->originY + cy) / 16.0f);
    if (col < 0) col = 0;
    text_queue(row, col, TXT_CYAN, "LEAVE ORBIT [D-PAD UP]");
}

/** playerTag is NULL for single-player's full status line, or a short tag
 *  like "P1" for multiplayer's compact per-quadrant one. */
static void draw_status_line(const Camera *cam, Ship *me, const char *playerTag) {
    char buf[64];
    if (playerTag != NULL) {
        const char *orbiting = (me != NULL && ship_in_orbit(me)) ? " ORBIT" : "";
        snprintf(buf, sizeof(buf), "%s K:%d%s", playerTag, me ? me->kills : 0, orbiting);
    } else {
        const char *orbiting = (me != NULL && ship_in_orbit(me)) ? "  IN ORBIT: REPAIR & REARM" : "";
        snprintf(buf, sizeof(buf), "PRACTICE ARENA  KILLS %d%s", me ? me->kills : 0, orbiting);
    }
    int localCol = (int)(cam->viewW / 8.0f / 2.0f) - (int)strlen(buf) / 2;
    if (localCol < 0) localCol = 0;
    int col = (int)(cam->originX / 8.0f) + localCol;
    int row = (int)(cam->originY / 16.0f);
    text_queue(row, col, TXT_WHITE, "%s", buf);
}

static void draw_overlay_gameover(const Camera *cam, int multiplayer) {
    int localRow = (int)(cam->viewH / 16.0f / 2.0f);
    int localCol = (int)(cam->viewW / 8.0f / 2.0f) - 7;
    if (localCol < 0) localCol = 0;
    int row = (int)(cam->originY / 16.0f) + localRow;
    int col = (int)(cam->originX / 8.0f) + localCol;
    text_queue(row, col, TXT_RED, "SHIP DESTROYED");
    const char *sub = multiplayer ? "spectating..." : "hull integrity lost -- press A";
    text_queue(row + 1, col - 6 < 0 ? 0 : col - 6, TXT_WHITE, "%s", sub);
}

void hud_draw(Hud *hud, const Camera *cam, GameWorld *world, Ship *me,
              const ShipInput *input, float time, int gameOver, const char *playerTag) {
    float scale = cam->viewH / 480.0f;
    if (me != NULL) {
        hud_layout(hud, (int)cam->viewW, (int)cam->viewH, &me->loadout);
        draw_bars(cam, me, scale);
        draw_radar(cam, world, me, time, scale);
        draw_buttons(cam, hud, world, me, input, time);
        if (ship_in_orbit(me)) draw_leave_orbit(cam, time, scale);
    }
    draw_status_line(cam, me, playerTag);
    if (gameOver) draw_overlay_gameover(cam, playerTag != NULL);
}
