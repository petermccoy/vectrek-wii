#include "loadout.h"
#include <string.h>

const WeaponInfo WEAPON_INFO[WEAPON_COUNT] = {
    [WEAPON_PROJECTILE] = { "Cannon",   "GUN",  3, 2, 1 },
    [WEAPON_ENERGY]     = { "Phaser",   "PHSR", 4, 2, 1 },
    [WEAPON_GUIDED]     = { "Missiles", "MSSL", 5, 2, 2 },
    [WEAPON_MINE]       = { "Mines",    "MINE", 4, 2, 1 },
};

const DefenseInfo DEFENSE_INFO[DEFENSE_COUNT] = {
    [DEFENSE_SHIELD] = { "Shields", "SHLD", 5, 2, 1 },
    [DEFENSE_CLOAK]  = { "Cloak",   "CLK",  4, 2, 1 },
};

const char *HULL_NAMES[3] = { "SABER", "CRUISER", "TALON" };

WeaponSpec weapon_spec(WeaponType type, int level) {
    WeaponSpec s = {0};
    switch (type) {
        case WEAPON_PROJECTILE:
            s = (WeaponSpec){ 8.0f + 3.0f * level, 40 + 15 * level, 0.24f, 0.0f, 575.0f, 1.8f, 0.0f };
            break;
        case WEAPON_ENERGY:
            // Phasers never miss, so they hit softer and recharge slowly; damage
            // is split again on the target: half to hull, half drains energy.
            s = (WeaponSpec){ 10.0f + 3.0f * level, -1, 1.2f, 12.0f - 1.5f * level, 0.0f, 0.0f, 620.0f + 60.0f * level };
            break;
        case WEAPON_GUIDED:
            s = (WeaponSpec){ 26.0f + 8.0f * level, 6 + 3 * level, 1.2f, 0.0f, 370.0f, 6.5f, 0.0f };
            break;
        case WEAPON_MINE:
            s = (WeaponSpec){ 34.0f + 10.0f * level, 5 + 3 * level, 0.8f, 0.0f, 0.0f, 90.0f, 0.0f };
            break;
        default:
            break;
    }
    return s;
}

float shield_absorb(int level) { return 0.70f + 0.08f * level; }
float shield_drain(int level) { return 7.0f - 1.5f * level; }
float cloak_drain(int level) { return 6.5f - 1.5f * level; }

int loadout_cost(const Loadout *lo) {
    int c = 0;
    for (int w = 0; w < WEAPON_COUNT; w++) {
        if (lo->weaponEquipped[w]) c += WEAPON_INFO[w].cost + WEAPON_INFO[w].upgradeCost * lo->weaponLevel[w];
    }
    for (int d = 0; d < DEFENSE_COUNT; d++) {
        if (lo->defenseEquipped[d]) c += DEFENSE_INFO[d].cost + DEFENSE_INFO[d].upgradeCost * lo->defenseLevel[d];
    }
    c += 2 * lo->energyLevel + lo->engineLevel + lo->radarLevel;
    return c;
}

Loadout loadout_default(void) {
    Loadout lo;
    memset(&lo, 0, sizeof(lo));
    lo.weaponEquipped[WEAPON_PROJECTILE] = 1;
    lo.weaponEquipped[WEAPON_GUIDED] = 1;
    lo.defenseEquipped[DEFENSE_SHIELD] = 1;
    lo.defenseEquipped[DEFENSE_CLOAK] = 1;
    lo.energyLevel = 1;
    lo.engineLevel = 0;
    lo.radarLevel = 1;
    lo.hullStyle = 0;
    return lo;
}

Loadout loadout_drone(void) {
    Loadout lo;
    memset(&lo, 0, sizeof(lo));
    lo.weaponEquipped[WEAPON_PROJECTILE] = 1;
    lo.hullStyle = 2;
    return lo;
}

ShipStats ship_stats_make(const Loadout *lo) {
    ShipStats s;
    s.maxHull = 100.0f;
    s.maxEnergy = 100.0f + 25.0f * lo->energyLevel;
    s.energyRegen = 6.0f + 1.5f * lo->energyLevel;
    // ~80% of the original pacing: slower, more deliberate flying.
    s.thrustAccel = 208.0f * (1.0f + 0.15f * lo->engineLevel);
    s.turnRate = 3.1f * (1.0f + 0.15f * lo->engineLevel);
    s.maxSpeed = 336.0f * (1.0f + 0.10f * lo->engineLevel);
    s.radarRange = 900.0f + 700.0f * lo->radarLevel;
    s.thrustDrain = 5.0f;
    return s;
}
