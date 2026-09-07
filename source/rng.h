// Small deterministic PRNG used for scenery generation and AI wandering.
// Doesn't need to match any particular platform's RNG bit-for-bit -- the Wii
// port has no networked state that depends on it, unlike the Android
// original's shared-seed multiplayer scenery.
#ifndef VECTREK_RNG_H
#define VECTREK_RNG_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} Rng;

static inline void rng_seed(Rng *r, uint32_t seed) {
    r->state = seed ? seed : 0x9E3779B9u;
}

static inline uint32_t rng_next_u32(Rng *r) {
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

/** [0, 1) */
static inline float rng_float(Rng *r) {
    return (float)(rng_next_u32(r) >> 8) / (float)(1u << 24);
}

/** [a, b) */
static inline float rng_range(Rng *r, float a, float b) {
    return a + rng_float(r) * (b - a);
}

/** [0, n) */
static inline int rng_int(Rng *r, int n) {
    return (int)(rng_float(r) * (float)n);
}

static inline int rng_bool(Rng *r) {
    return rng_next_u32(r) & 1u;
}

#endif
