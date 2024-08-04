#ifndef HI_NOISE_H
#define HI_NOISE_H

// A 2D perlin noise generator. This generates "random" numbers with natural
// looking gradients for points that are close together.
// See https://en.wikipedia.org/wiki/Perlin_noise
// FIXME: should this even be part of high_impact?

#include "types.h"

typedef struct noise_t noise_t;

// Bump allcoate and create a noise generator with a size of 1 << size_bits
noise_t *noise(uint8_t size_bits);

// Get the noise value in the range of -1..1
float noise_gen(noise_t *n, vec2_t pos);

#endif
