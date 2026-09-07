// Points-based outfitting system. Ported from Loadout.kt.
#ifndef VECTREK_LOADOUT_H
#define VECTREK_LOADOUT_H

typedef enum {
    WEAPON_PROJECTILE = 0,
    WEAPON_ENERGY,
    WEAPON_GUIDED,
    WEAPON_MINE,
    WEAPON_COUNT,
} WeaponType;

typedef enum {
    DEFENSE_SHIELD = 0,
    DEFENSE_CLOAK,
    DEFENSE_COUNT,
} DefenseType;

typedef struct {
    const char *label;
    const char *shortLabel;
    int cost;
    int maxLevel;
    int upgradeCost;
} WeaponInfo;

typedef struct {
    const char *label;
    const char *shortLabel;
    int cost;
    int maxLevel;
    int upgradeCost;
} DefenseInfo;

extern const WeaponInfo WEAPON_INFO[WEAPON_COUNT];
extern const DefenseInfo DEFENSE_INFO[DEFENSE_COUNT];

typedef struct {
    float damage;      // phaser: total damage, split among all locked targets
    int ammo;           // -1 = unlimited (energy weapon)
    float cooldown;     // seconds between shots
    float energyCost;   // per shot
    float speed;        // muzzle speed added to ship velocity
    float life;         // seconds before the shot expires
    float range;        // phaser beam lock range
} WeaponSpec;

WeaponSpec weapon_spec(WeaponType type, int level);

/** Shield: fraction of a hit absorbed while raised; energy pays for what it soaks. */
float shield_absorb(int level);
float shield_drain(int level);   // per second while raised
float cloak_drain(int level);    // per second while cloaked

#define LOADOUT_BUDGET 20

extern const char *HULL_NAMES[3];
#define HULL_NAME_COUNT 3

typedef struct {
    int weaponEquipped[WEAPON_COUNT];
    int weaponLevel[WEAPON_COUNT];
    int defenseEquipped[DEFENSE_COUNT];
    int defenseLevel[DEFENSE_COUNT];
    int energyLevel;   // 0..3: +25 max energy and +1.5 regen per level (2 pts each)
    int engineLevel;   // 0..2: +15% thrust/turn/speed per level (1 pt each)
    int radarLevel;    // 0..3: +700 radar range per level (1 pt each)
    int hullStyle;     // cosmetic, free: index into HULL_NAMES
} Loadout;

int loadout_cost(const Loadout *lo);
static inline int loadout_has_weapon(const Loadout *lo, WeaponType w) { return lo->weaponEquipped[w]; }
static inline int loadout_has_defense(const Loadout *lo, DefenseType d) { return lo->defenseEquipped[d]; }
static inline int loadout_weapon_level(const Loadout *lo, WeaponType w) { return lo->weaponEquipped[w] ? lo->weaponLevel[w] : 0; }
static inline int loadout_defense_level(const Loadout *lo, DefenseType d) { return lo->defenseEquipped[d] ? lo->defenseLevel[d] : 0; }

/** Cannon + missiles + shield + cloak + energy I + radar I = 20 pts. */
Loadout loadout_default(void);
/** Cheap fit used by practice drones. */
Loadout loadout_drone(void);

typedef struct {
    float maxHull;
    float maxEnergy;
    float energyRegen;
    float thrustAccel;
    float turnRate;
    float maxSpeed;
    float radarRange;
    float thrustDrain;
} ShipStats;

ShipStats ship_stats_make(const Loadout *lo);

#endif
