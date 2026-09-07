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

#define DEFAULT_FIFO_SIZE (256 * 1024)

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static u8 gp_fifo[DEFAULT_FIFO_SIZE] ATTRIBUTE_ALIGN(32);

typedef enum { APP_MAIN_MENU, APP_LOADOUT, APP_GAME } AppState;

static GameWorld world;
static int playerId = -1;
static int gameOver = 0;
static float droneTimer = 0.0f;
static int droneCounter = 0;
static Loadout playerLoadout;
static Camera cam;
static Hud hud;

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
    render_init(&cam, (float)rmode->fbWidth, (float)rmode->efbHeight);
    hud_init(&hud);
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
                render_draw_world(&cam, &world, me2, timeAccum);
                hud_draw(&hud, &cam, &world, me2, &frameInput, timeAccum, gameOver);
                drewGx = 1;
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
