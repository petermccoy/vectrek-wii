# VecTrek -- Wii

A homebrew Wii port of [VecTrek](https://github.com/petermccoy/vectrek), a
2D space combat simulator with old-school vector graphics, inspired by
Asteroids, Omega Race and Netrek. Same physics, same weapons, same
outfitting system -- reimplemented in C over libogc/devkitPPC with GX for
the vector-style rendering and a Wiimote in place of a touchscreen.

Pure C, no third-party libraries beyond the devkitPro Wii toolchain
(libogc + wiiuse for the GX/video/Wiimote layers).

## How it plays

Same rules as the original: point your ship somewhere and it turns to face
that point and burns while you hold; release to coast on inertia. The
arena is a large bounded battlefield littered with drifting asteroids,
suns with 1-4 orbiting planets each, and paired wormholes. Suns, planets
and wormholes pull on ships and shots with real gravity; touching a sun is
instant death; wormholes fling you out of their twin; brushing a planet
parks you in a repair orbit that mends your hull and restocks ammunition.

Weapons (Cannon, Phaser, Missiles, Mines) and defenses (Shields, Cloak)
work exactly as in the Android version, including the phaser's
lock-everyone-in-range-and-split-the-damage behavior and cloak dropping the
moment you fire (mines excepted). Ships are built from the same 20-point
outfitting budget in the OUTFITTING BAY.

This port ships two ways to play:

- **Practice Arena**: solo, full-screen, against respawning AI drones (the
  same wandering/chasing/firing brain as the Android build).
- **Multiplayer**: local split-screen for 2-4 players, one Wiimote each,
  pilot-vs-pilot, no drones. See [Multiplayer](#multiplayer) below.

See [Not yet ported](#not-yet-ported) for what's still missing relative to
the Android app (LAN play, chiefly).

## Controls

The Android version steers by touch: touch anywhere in space and the ship
turns toward that point and thrusts while held. **Solo play** uses the
Wiimote's IR pointer as the closest hardware equivalent to that:

| Input (solo) | Action |
|---|---|
| Point at the screen + hold **B** (trigger) | Turn toward the pointer and burn -- release to coast |
| **A** | Fire weapon slot 1 (first equipped weapon) |
| **1** | Fire weapon slot 2 |
| **2** | Fire weapon slot 3 |
| **-** | Toggle shields |
| **+** | Toggle cloak |
| **D-Pad Up** | Leave orbit (while parked at a planet) |
| **HOME** | Quit to the system menu |

**Multiplayer** can't use the IR pointer -- with the screen split into
quadrants, one absolute pointer position on the shared sensor bar doesn't
say which player's quadrant it belongs to. Each Wiimote instead steers
directly, D-pad style:

| Input (multiplayer, per Wiimote) | Action |
|---|---|
| **D-Pad Left / Right** | Turn left / right |
| **D-Pad Up** (hold) | Thrust |
| **D-Pad Down** | Leave orbit |
| **A** or **B** | Fire weapon slot 1 |
| **1** | Fire weapon slot 2 |
| **2** | Fire weapon slot 3 |
| **-** | Toggle shields |
| **+** | Toggle cloak |

In both modes, weapon slots are assigned in the same order the HUD buttons
are laid out in, so the on-screen `[A]`/`[1]`/`[2]` labels next to each
weapon always match. Solo play requires a Sensor Bar for the IR pointer
(as with any Wii software that uses one); multiplayer doesn't need one.

Menus (main menu, outfitting bay, multiplayer setup) use the D-Pad to
move, **A** to confirm/toggle, **Left/Right** to adjust a level or cycle a
value, and **B** to cancel.

## Multiplayer

Pick **MULTIPLAYER (SPLIT-SCREEN)** from the main menu, choose 2-4 players
with D-Pad Left/Right, sync that many Wiimotes (the setup screen shows
which channels it sees), and press **A** to start. All players fly the
loadout currently saved in the OUTFITTING BAY -- there's no per-player fit
yet, so agree on one before starting, or just go with the default.

The screen splits top/bottom for 2 players, or into a 2x2 grid for 3-4 (the
4th cell sits empty for a 3-player match). Everyone shares one arena, one
set of hazards, and no AI drones -- it's pilot vs. pilot. A destroyed
ship's quadrant freezes on its last view and shows `SPECTATING`; the match
ends the moment one player is left standing (or everyone's dead at once),
and a results screen ranks everyone by kills before returning to the main
menu.

## Building

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the
`wii-dev` package group (devkitPPC + libogc + libwiiuse), and `DEVKITPRO`/
`DEVKITPPC` set in your environment (the devkitPro installer does this for
you). Then, from this directory:

```
make
```

This produces `vectrek-wii.dol` (load via the Homebrew Channel, an SD
card loader, or an emulator like Dolphin) and `vectrek-wii.elf`.

CI (`.github/workflows/build.yml`) builds the same target on every push
using the official `devkitpro/devkitppc` Docker image, so a broken build
shows up on GitHub even without a local toolchain.

## Code map

```
source/
├── vec2.h            vector math
├── rng.h             small deterministic PRNG (scenery gen, AI wander)
├── loadout.h/.c       points system, weapon/defense specs, derived ship stats
├── entity.h/.c        Ship (energy/hull/weapons/defenses) and Shot update logic
├── scenery.h/.c       asteroids, stars, planets, wormholes, explosions, beams
├── aicontroller.h/.c  practice-drone brain
├── gameworld.h/.c      world generation, gravity, stepping, collisions
├── gfx.h/.c            minimal GX 2D immediate-mode primitives (lines, circles, arcs)
├── palette.h/.c        the vector-look color palette
├── render.h/.c         world rendering (camera, starfield, scenery, ships, effects)
├── text.h/.c           HUD/menu text via libogc's console (deferred-draw queue)
├── hud.h/.c            bars, radar, weapon/defense buttons, status line
├── input.h/.c          Wiimote -> pilot intent
├── menu.h/.c           main menu, outfitting bay, multiplayer setup
└── main.c              video/GX/WPAD init, app state machine, game loop
```

`vec2.h` through `aicontroller.c` are a line-for-line port of the Android
app's `engine/` package (same constants, same formulas); `gfx.c` upward is
a from-scratch Wii-native replacement for the Android `ui/` package
(Canvas/Paint rendering, touch input, and the Activity-based menus).

## Not yet ported

- **LAN multiplayer.** The Android version's host/join battles use
  Android's NSD (mDNS) for discovery over UDP. Wii homebrew networking
  (libogc's BSD-style sockets over the console's own Wi-Fi/USB adapter)
  is a different enough transport and discovery story that it's left as
  a follow-up rather than guessed at here; the engine code (`gameworld.c`
  et al.) doesn't care who's driving the ship, so wiring up a host/client
  protocol on top of it later doesn't require touching the simulation.
  Split-screen local multiplayer (2-4 players, one Wiimote each) is
  ported -- see [Multiplayer](#multiplayer).
- **Per-player loadouts.** Split-screen multiplayer gives every player the
  same shared loadout (whatever's saved in the OUTFITTING BAY); the
  Android app only ever had one player's loadout to begin with, so this
  is new ground rather than a gap versus Android. A per-player outfitting
  step before a match would be the natural next step.
- **Sound.** The Android build has none either (see its own roadmap);
  this port doesn't add any.
- **Persistent loadout.** The outfitting bay's saved fit lives in memory
  for the current run only -- there's no SD-card save yet (the Android
  version uses SharedPreferences, which has no Wii equivalent wired up
  here).

## A note on how this was built

This port could not be compiled or run against real hardware/an emulator
in the environment it was written in (no devkitPPC toolchain, and the
sandbox's network policy blocks devkitPro's package repository). The
GitHub Actions workflow in this repo is the first real compile of it --
treat early commits on this branch accordingly, and check the Actions tab
for the current build status before flashing anything to a real Wii or SD
card.
