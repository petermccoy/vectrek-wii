// VecTrek -- Wii homebrew port. State machine: main menu -> outfitting bay ->
// practice arena (vs. respawning AI drones) -> game over -> main menu.
// Ported from MainActivity/LoadoutActivity/GameSession/GameView.kt.
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec2.h"
#include "loadout.h"
#include "entity.h"
#include "gameworld.h"
#include "aicontroller.h"
#include "render.h"
#include "hud.h"
#include "input.h"
#include "menu.h"
#include "text.h"
#include "gfx.h"
#include "palette.h"

#define DEFAULT_FIFO_SIZE (256 * 1024)

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);

typedef enum { APP_MAIN_MENU, APP_LOADOUT, APP_GAME, APP_MP_SETUP, APP_MP_GAME, APP_MP_RESULTS } AppState;

static GameWorld world;
static int playerId = -1;
static int gameOver = 0;
static float droneTimer = 0.0f;
static int droneCounter = 0;
static Loadout playerLoadout;
static Camera cam;
static Hud hud;

// Split-screen multiplayer: up to MP_MAX_PLAYERS human pilots sharing one
// world, each with their own Wiimote, camera and HUD confined to a screen
// quadrant. No AI drones -- pure pilot-vs-pilot.
static int mpPlayerCount = 0;
static int mpShipId[MP_MAX_PLAYERS];
static Camera mpCam[MP_MAX_PLAYERS];
static Hud mpHud[MP_MAX_PLAYERS];
static ShipInput mpInput[MP_MAX_PLAYERS];
static char mpTag[MP_MAX_PLAYERS][4];
// Snapshot of kills/alive taken every frame while a ship still exists --
// once destroyed a ship is dropped from the world entirely, so this is the
// only place its final standing survives to the results screen.
static int mpKills[MP_MAX_PLAYERS];
static int mpAliveFlag[MP_MAX_PLAYERS];

static Ship *find_ship(int id) {
    for (int i = 0; i < world.shipCount; i++) {
        if (world.ships[i].id == id) return &world.ships[i];
    }
    return NULL;
}

static void spawn_drone(void) {
    droneCounter++;
    char name[SHIP_NAME_LEN];
    snprintf(name, sizeof(name), "DRONE-%d", droneCounter);
    Loadout dlo = loadout_drone();
    Ship *me = find_ship(playerId);
    Vec2 away = me ? me->pos : vec2_zero();
    Vec2 spawnPos = gameworld_find_spawn_point(&world, me ? &away : NULL);
    Ship *drone = gameworld_add_ship(&world, name, &dlo, &spawnPos);
    if (drone) ai_init(drone, (unsigned long)droneCounter * 7919UL);
}

static void update_drones(float dt) {
    int drones = 0;
    for (int i = 0; i < world.shipCount; i++) {
        if (world.ships[i].isAi && world.ships[i].alive) drones++;
    }
    if (drones < 3) {
        droneTimer -= dt;
        if (droneTimer <= 0.0f) {
            spawn_drone();
            droneTimer = 6.0f;
        }
    }
}

static void start_practice_session(void) {
    gameworld_init(&world, (unsigned long)gettime(), 8000.0f, 6000.0f);
    gameworld_generate(&world);
    Ship *p = gameworld_add_ship(&world, "PILOT", &playerLoadout, NULL);
    playerId = p ? p->id : -1;
    gameOver = 0;
    droneTimer = 4.0f;
    droneCounter = 0;
    spawn_drone();
    spawn_drone();
    render_init(&cam, 0.0f, 0.0f, (float)rmode->fbWidth, (float)rmode->efbHeight);
    hud_init(&hud);
}

/** Screen quadrant for player `idx` of `count`: 2 players split top/bottom;
 *  3-4 players use a 2x2 grid (3 players just leaves the 4th cell empty). */
static void get_viewport_rect(int idx, int count, int fbW, int fbH, int *vx, int *vy, int *vw, int *vh) {
    if (count <= 1) {
        *vx = 0; *vy = 0; *vw = fbW; *vh = fbH;
        return;
    }
    if (count == 2) {
        *vw = fbW; *vh = fbH / 2;
        *vx = 0; *vy = idx * (*vh);
        return;
    }
    *vw = fbW / 2; *vh = fbH / 2;
    *vx = (idx % 2) * (*vw);
    *vy = (idx / 2) * (*vh);
}

/** Point GX at a pixel sub-rectangle of the framebuffer: viewport, scissor
 *  and a matching orthographic projection so viewport-local coordinates
 *  (0,0)-(vw,vh) fill exactly that sub-rect. */
static void gx_set_viewport(int vx, int vy, int vw, int vh) {
    GX_SetViewport((f32)vx, (f32)vy, (f32)vw, (f32)vh, 0, 1);
    GX_SetScissor((u32)vx, (u32)vy, (u32)vw, (u32)vh);
    Mtx44 proj;
    guOrtho(proj, 0, (f32)vh, 0, (f32)vw, 0, 300);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

/** Thin separator lines between quadrants, drawn full-screen after all
 *  viewports so they aren't clipped by any one player's scissor rect. */
static void draw_split_dividers(int count, int fbW, int fbH) {
    if (count < 2) return;
    GXColor c = pal_alpha(PAL_BOUNDS, 200);
    gfx_line_width(2.0f);
    if (count == 2) {
        gfx_line(0.0f, fbH / 2.0f, (float)fbW, fbH / 2.0f, c);
    } else {
        gfx_line(fbW / 2.0f, 0.0f, fbW / 2.0f, (float)fbH, c);
        gfx_line(0.0f, fbH / 2.0f, (float)fbW, fbH / 2.0f, c);
    }
}

static void start_multiplayer_session(int count) {
    gameworld_init(&world, (unsigned long)gettime(), 8000.0f, 6000.0f);
    gameworld_generate(&world);
    mpPlayerCount = count;
    for (int i = 0; i < count; i++) {
        snprintf(mpTag[i], sizeof(mpTag[i]), "P%d", i + 1);
        Ship *p = gameworld_add_ship(&world, mpTag[i], &playerLoadout, NULL);
        mpShipId[i] = p ? p->id : -1;
        mpKills[i] = 0;
        mpAliveFlag[i] = 1;

        int vx, vy, vw, vh;
        get_viewport_rect(i, count, rmode->fbWidth, rmode->efbHeight, &vx, &vy, &vw, &vh);
        render_init(&mpCam[i], (float)vx, (float)vy, (float)vw, (float)vh);
        hud_init(&mpHud[i]);
        memset(&mpInput[i], 0, sizeof(mpInput[i]));
    }
}

static void draw_mp_results(void) {
    text_color(TXT_CYAN);
    text_at(3, 28, "MATCH RESULTS");

    // Selection sort by kills, descending -- at most 4 entries.
    int order[MP_MAX_PLAYERS];
    for (int i = 0; i < mpPlayerCount; i++) order[i] = i;
    for (int i = 0; i < mpPlayerCount; i++) {
        for (int j = i + 1; j < mpPlayerCount; j++) {
            if (mpKills[order[j]] > mpKills[order[i]]) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
        }
    }
    for (int row = 0; row < mpPlayerCount; row++) {
        int i = order[row];
        text_color(mpAliveFlag[i] ? TXT_GREEN : TXT_WHITE);
        text_at(7 + row, 24, "%-4s KILLS %-3d %s", mpTag[i], mpKills[i], mpAliveFlag[i] ? "SURVIVED" : "DESTROYED");
    }
    text_color(TXT_WHITE);
    text_at(20, 24, "press A to return to menu");
}

static void gx_init(void) {
    memset(gp_fifo, 0, DEFAULT_FIFO_SIZE);
    GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

    GXColor background = { 0, 0, 0, 0xff };
    GX_SetCopyClear(background, GX_MAX_Z24);

    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopyYScale((f32)rmode->xfbHeight / (f32)rmode->efbHeight);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, (rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_CopyDisp(xfb, GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    // Orthographic 2D projection matching the framebuffer, pixel for pixel,
    // origin top-left with Y increasing downward (same convention the
    // Android original's Canvas used).
    Mtx44 proj;
    guOrtho(proj, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    Mtx modelview;
    guMtxIdentity(modelview);
    guMtxTransApply(modelview, modelview, 0, 0, -1);
    GX_LoadPosMtxImm(modelview, GX_PNMTX0);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetNumChans(1);
    // Without this, channel 0's ambient/material source is whatever GX_Init
    // happened to leave it at -- unreliable, and the likely cause of the
    // solid-color/garbled screen some builds showed. Pin it explicitly to
    // "no lighting, output the per-vertex color untouched".
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetNumTevStages(1);
}

int main(int argc, char **argv) {
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    text_init(xfb, rmode);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    input_init(rmode->fbWidth, rmode->efbHeight);
    gx_init();

    // PAL runs at 50Hz instead of NTSC/60Hz -- keep sim speed independent of TV standard.
    float dt = ((rmode->viTVMode >> 2) == VI_PAL) ? (1.0f / 50.0f) : (1.0f / 60.0f);

    playerLoadout = loadout_default();
    AppState state = APP_MAIN_MENU;
    menu_main_init();

    ShipInput frameInput;
    memset(&frameInput, 0, sizeof(frameInput));
    float timeAccum = 0.0f;
    int running = 1;

    while (running) {
        timeAccum += dt;

        WPAD_ScanPads();
        u32 held0 = WPAD_ButtonsHeld(WPAD_CHAN_0);
        if (held0 & WPAD_BUTTON_HOME) break;

        int drewGx = 0;

        switch (state) {
            case APP_MAIN_MENU: {
                MenuAction a = menu_main_update();
                if (a == MENU_ACTION_START) {
                    start_practice_session();
                    state = APP_GAME;
                } else if (a == MENU_ACTION_MULTIPLAYER) {
                    mp_setup_menu_init();
                    state = APP_MP_SETUP;
                } else if (a == MENU_ACTION_OUTFIT) {
                    loadout_menu_init(&playerLoadout);
                    state = APP_LOADOUT;
                } else if (a == MENU_ACTION_EXIT) {
                    running = 0;
                }
                break;
            }
            case APP_LOADOUT: {
                Loadout saved;
                LoadoutAction a = loadout_menu_update(&saved);
                if (a == LOADOUT_ACTION_SAVE) {
                    playerLoadout = saved;
                    menu_main_init();
                    state = APP_MAIN_MENU;
                } else if (a == LOADOUT_ACTION_CANCEL) {
                    menu_main_init();
                    state = APP_MAIN_MENU;
                }
                break;
            }
            case APP_GAME: {
                if (gameOver) {
                    if (held0 & WPAD_BUTTON_A) {
                        menu_main_init();
                        state = APP_MAIN_MENU;
                        break;
                    }
                } else {
                    input_poll(&frameInput, &cam, &playerLoadout);
                    Ship *me = find_ship(playerId);
                    if (me == NULL) {
                        gameOver = 1;
                    } else {
                        me->input = frameInput;
                        gameworld_step(&world, dt);
                        update_drones(dt);
                    }
                }
                Ship *me2 = find_ship(playerId);
                // Multiplayer leaves GX pointed at a quadrant; single-player
                // always renders full-screen.
                gx_set_viewport(0, 0, rmode->fbWidth, rmode->efbHeight);
                render_draw_world(&cam, &world, me2, timeAccum);
                hud_draw(&hud, &cam, &world, me2, &frameInput, timeAccum, gameOver, NULL);
                drewGx = 1;
                break;
            }
            case APP_MP_SETUP: {
                int count;
                MpSetupAction a = mp_setup_menu_update(&count);
                if (a == MP_SETUP_ACTION_START) {
                    start_multiplayer_session(count);
                    state = APP_MP_GAME;
                } else if (a == MP_SETUP_ACTION_CANCEL) {
                    menu_main_init();
                    state = APP_MAIN_MENU;
                }
                break;
            }
            case APP_MP_GAME: {
                for (int i = 0; i < mpPlayerCount; i++) {
                    Ship *s = find_ship(mpShipId[i]);
                    if (s != NULL && s->alive) {
                        input_poll_mp(i, &mpInput[i], &playerLoadout);
                        s->input = mpInput[i];
                    }
                }
                gameworld_step(&world, dt);

                int aliveCount = 0;
                for (int i = 0; i < mpPlayerCount; i++) {
                    int vx, vy, vw, vh;
                    get_viewport_rect(i, mpPlayerCount, rmode->fbWidth, rmode->efbHeight, &vx, &vy, &vw, &vh);
                    gx_set_viewport(vx, vy, vw, vh);

                    Ship *s = find_ship(mpShipId[i]);
                    if (s != NULL) {
                        mpKills[i] = s->kills;
                        mpAliveFlag[i] = 1;
                        aliveCount++;
                    } else {
                        mpAliveFlag[i] = 0;
                    }
                    render_draw_world(&mpCam[i], &world, s, timeAccum);
                    hud_draw(&mpHud[i], &mpCam[i], &world, s, &mpInput[i], timeAccum, s == NULL, mpTag[i]);
                }

                gx_set_viewport(0, 0, rmode->fbWidth, rmode->efbHeight);
                draw_split_dividers(mpPlayerCount, rmode->fbWidth, rmode->efbHeight);
                drewGx = 1;

                if (aliveCount <= 1) state = APP_MP_RESULTS;
                break;
            }
            case APP_MP_RESULTS: {
                draw_mp_results();
                if (held0 & WPAD_BUTTON_A) {
                    menu_main_init();
                    state = APP_MAIN_MENU;
                }
                break;
            }
        }

        if (drewGx) {
            GX_DrawDone();
            GX_CopyDisp(xfb, GX_TRUE);
            text_flush(); // labels land on top of the just-copied frame
        }
        VIDEO_SetNextFramebuffer(xfb);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

    return 0;
}
