#ifndef HI_TRACE_H
#define HI_TRACE_H

// Trace "sweeps" an axis aligned box over a map (usually a collision_map) and
// returns the first position and further information of a hit.

#include "types.h"
#include "map.h"

typedef struct {
	// The tile that was hit. 0 if no hit. 
	int tile;

	// The tile position (in tile space) of the hit
	vec2i_t tile_pos;

	// The normalized 0..1 length of this trace. If this trace did not end in
	// a hit, length will be 1.
	float length;

	// The resulting position of the top left corne of the AABB that was traced
	vec2_t pos;

	// The normal vector of the surface that was hit
	vec2_t normal;
} trace_t;

// Trace map with the AABB's top left corner, the movement vecotr and size
trace_t trace(map_t *map, vec2_t from, vec2_t vel, vec2_t size);

#endif
