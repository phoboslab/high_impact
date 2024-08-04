#include "noise.h"
#include "alloc.h"
#include "utils.h"

struct noise_t {
	int size_bits;
	vec2_t *g;
	uint16_t *p;
};

noise_t *noise(uint8_t size_bits) {
	error_if(size_bits > 15, "Max noise size bits");
	noise_t *n = bump_alloc(sizeof(noise_t));
	n->size_bits = size_bits;

	uint16_t size = 1 << size_bits;
	n->g = bump_alloc(sizeof(vec2_t) * size);
	n->p = bump_alloc(sizeof(uint16_t *) * size);

	for (int i = 0; i < size; i++) {
		n->g[i] = vec2(rand_float(-1, 1), rand_float(-1, 1));
		n->p[i] = i;
	}
	
	shuffle(n->p, size);
	return n;
}

float noise_gen(noise_t *n, vec2_t pos) {
	int size = 1 << n->size_bits;
	int mask = size - 1;

	uint16_t *p = n->p;
	vec2_t *g = n->g;

	// Compute what gradients to use
	int qx0 = (int)pos.x & mask;
	int qx1 = (qx0 + 1) & mask;
	float tx0 = pos.x - qx0;
	float tx1 = tx0 - 1;

	int qy0 = (int)pos.y & mask;
	int qy1 = (qy0 + 1) & mask;
	float ty0 = pos.y - qy0;
	float ty1 = ty0 - 1;

	// Permutate values to get pseudo randomly chosen gradients
	int q00 = p[(qy0 + p[qx0]) & mask];
	int q01 = p[(qy0 + p[qx1]) & mask];

	int q10 = p[(qy1 + p[qx0]) & mask];
	int q11 = p[(qy1 + p[qx1]) & mask];

	// Compute the dotproduct between the vectors and the gradients
	float v00 = g[q00].x * tx0 + g[q00].y * ty0;
	float v01 = g[q01].x * tx1 + g[q01].y * ty0;

	float v10 = g[q10].x * tx0 + g[q10].y * ty1;
	float v11 = g[q11].x * tx1 + g[q11].y * ty1;

	// Modulate with the weight function
	float wx = (3 - 2 * tx0) * tx0 * tx0;
	float v0 = v00 - wx*(v00 - v01);
	float v1 = v10 - wx*(v10 - v11);

	float wy = (3 - 2 * ty0) * ty0 * ty0;
	float v = v0 - wy*(v0 - v1);

	return v;
}