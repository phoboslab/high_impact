#include "trace.h"
#include "alloc.h"
#include "utils.h"

typedef struct {
	vec2_t start;
	vec2_t dir;
	vec2_t normal;
	bool solid;
} slope_def_t;

// Define all sloped tiles by their start (x,y) and end (x,y) coordinates in
// normalized (0..1) space. We compute the direction of the slope and the
// slope's normal from this.

// Some compilers allow the use of sqrt() in a constant initializer, but others
// do not. So lets have some fun with macros to compute the square roots. With 
// 3 iterations of Newton's method. this is reasonably accurate for the needed 
// range of 0..1
#define SQRT_ITER(N, GUESS) ((GUESS + ((N) / (GUESS))) * 0.5)
#define SQRT(N) (SQRT_ITER((N), SQRT_ITER((N), SQRT_ITER((N), (N)))))

#define SLOPE_LEN(X, Y) (SQRT((X) * (X) + (Y) * (Y)))
#define SLOPE_NORMAL(X, Y) {((Y) / SLOPE_LEN((X), (Y))), (-(X) / SLOPE_LEN((X), (Y)))}

#define SLOPE(SX, SY, EX, EY, SOLID) { \
	.start = {SX, SY},\
	.dir = {EX - SX, EY - SY}, \
	.normal = SLOPE_NORMAL(EX - SX, EY - SY), \
	.solid = SOLID \
}

// Corner points for all slope tiles are either at 0.0, 1.0, 0.5, 0.333 or 0.666
// Defining these here as H, N and M hopefully makes this a bit easier to read.

#define H (1.0 / 2.0)
#define N (1.0 / 3.0)
#define M (2.0 / 3.0)
#define SOLID true
#define ONE_WAY false

static const slope_def_t slope_definitions[] = {
	/*     15 NE */ [ 5] = SLOPE(0,1, 1,M, SOLID), [ 6] = SLOPE(0,M, 1,N, SOLID), [ 7] = SLOPE(0,N, 1,0, SOLID),
	/*     22 NE */ [ 3] = SLOPE(0,1, 1,H, SOLID), [ 4] = SLOPE(0,H, 1,0, SOLID),
	/*     45 NE */ [ 2] = SLOPE(0,1, 1,0, SOLID),
	/*     67 NE */ [10] = SLOPE(H,1, 1,0, SOLID), [21] = SLOPE(0,1, H,0, SOLID),
	/*     75 NE */ [32] = SLOPE(M,1, 1,0, SOLID), [43] = SLOPE(N,1, M,0, SOLID), [54] = SLOPE(0,1, N,0, SOLID),
	
	/*     15 SE */ [27] = SLOPE(0,0, 1,N, SOLID), [28] = SLOPE(0,N, 1,M, SOLID), [29] = SLOPE(0,M, 1,1, SOLID),
	/*     22 SE */ [25] = SLOPE(0,0, 1,H, SOLID), [26] = SLOPE(0,H, 1,1, SOLID),
	/*     45 SE */ [24] = SLOPE(0,0, 1,1, SOLID),
	/*     67 SE */ [11] = SLOPE(0,0, H,1, SOLID), [22] = SLOPE(H,0, 1,1, SOLID),
	/*     75 SE */ [33] = SLOPE(0,0, N,1, SOLID), [44] = SLOPE(N,0, M,1, SOLID), [55] = SLOPE(M,0, 1,1, SOLID),
	
	/*     15 NW */ [16] = SLOPE(1,N, 0,0, SOLID), [17] = SLOPE(1,M, 0,N, SOLID), [18] = SLOPE(1,1, 0,M, SOLID),
	/*     22 NW */ [14] = SLOPE(1,H, 0,0, SOLID), [15] = SLOPE(1,1, 0,H, SOLID),
	/*     45 NW */ [13] = SLOPE(1,1, 0,0, SOLID),
	/*     67 NW */ [ 8] = SLOPE(H,1, 0,0, SOLID), [19] = SLOPE(1,1, H,0, SOLID),
	/*     75 NW */ [30] = SLOPE(N,1, 0,0, SOLID), [41] = SLOPE(M,1, N,0, SOLID), [52] = SLOPE(1,1, M,0, SOLID),
	
	/*     15 SW */ [38] = SLOPE(1,M, 0,1, SOLID), [39] = SLOPE(1,N, 0,M, SOLID), [40] = SLOPE(1,0, 0,N, SOLID),
	/*     22 SW */ [36] = SLOPE(1,H, 0,1, SOLID), [37] = SLOPE(1,0, 0,H, SOLID),
	/*     45 SW */ [35] = SLOPE(1,0, 0,1, SOLID),
	/*     67 SW */ [ 9] = SLOPE(1,0, H,1, SOLID), [20] = SLOPE(H,0, 0,1, SOLID),
	/*     75 SW */ [31] = SLOPE(1,0, M,1, SOLID), [42] = SLOPE(M,0, N,1, SOLID), [53] = SLOPE(N,0, 0,1, SOLID),
	
	/* One way N */ [12] = SLOPE(0,0, 1,0, ONE_WAY),
	/* One way S */ [23] = SLOPE(1,1, 0,1, ONE_WAY),
	/* One way E */ [34] = SLOPE(1,0, 1,1, ONE_WAY),
	/* One way W */ [45] = SLOPE(0,1, 0,0, ONE_WAY)
};


static inline void check_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res);
static void resolve_full_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res);
static void resolve_sloped_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, uint32_t tile, trace_t *res);

trace_t trace(map_t *map, vec2_t from, vec2_t vel, vec2_t size) {
	vec2_t to = vec2_add(from, vel);

	trace_t res = {
		.tile = 0,
		.pos = to,
		.normal = vec2(0, 0),
		.length = 1
	};

	// Quick check if the whole trace is out of bounds
	vec2i_t map_size_px = vec2i_muli(map->size, map->tile_size);
	if (
		(from.x + size.x < 0 && to.x + size.x < 0) ||
		(from.y + size.y < 0 && to.y + size.y < 0) ||
		(from.x > map_size_px.x && to.x > map_size_px.x) ||
		(from.y > map_size_px.y && to.y > map_size_px.y) ||
		(vel.x == 0 && vel.y == 0)
	) {
		return res;
	}

	vec2_t offset = vec2(
		vel.x > 0 ? 1 : 0,
		vel.y > 0 ? 1 : 0
	);
	vec2_t corner = vec2_add(from, vec2_mul(size, offset));
	vec2_t dir = vec2_add(vec2_mulf(offset, -2), vec2(1, 1));

	float max_vel = max(vel.x * -dir.x, vel.y * -dir.y);
	int steps = ceil(max_vel / (float)map->tile_size);
	if (steps == 0) {
		return res;
	}
	vec2_t step_size = vec2_divf(vel, steps);

	vec2i_t last_tile_pos = vec2i(-16, -16);
	bool extra_step_for_slope = false;
	for (int i = 0; i <= steps; i++) {
		vec2i_t tile_pos = vec2i_from_vec2(vec2_divf(vec2_add(corner, vec2_mulf(step_size, i)), map->tile_size));
		
		int corner_tile_checked = 0;
		if (last_tile_pos.x != tile_pos.x) {
			// Figure out the number of tiles in Y direction we need to check.
			// This walks along the vertical edge of the object (height) from
			// the current tile_pos.x,tile_pos.y position.
			float max_y = from.y + size.y * (1 - offset.y);
			if (i > 0) {
				max_y += (vel.y / vel.x) * ((tile_pos.x + 1 - offset.x) * map->tile_size - corner.x);
			}

			int num_tiles = ceilf(fabsf(max_y / map->tile_size - tile_pos.y - offset.y));
			for (int t = 0; t < num_tiles; t++) {
				check_tile(map, from, vel, size, vec2i(tile_pos.x, tile_pos.y + dir.y * t), &res);
			}

			last_tile_pos.x = tile_pos.x;
			corner_tile_checked = 1;
		}

		if (last_tile_pos.y != tile_pos.y) {
			// Figure out the number of tiles in X direction we need to
			// check. This walks along the horizontal edge of the object 
			// (width) from the current tile_pos.x,tile_pos.y position.
			float max_x = from.x + size.x * (1 - offset.x);
			if (i > 0) {
				max_x += (vel.x / vel.y) * ((tile_pos.y + 1 - offset.y) * map->tile_size - corner.y);
			}
			
			int num_tiles = ceilf(fabsf(max_x / map->tile_size - tile_pos.x - offset.x));
			for (int t = corner_tile_checked; t < num_tiles; t++) {
				check_tile(map, from, vel, size, vec2i(tile_pos.x + dir.x * t, tile_pos.y), &res);
			}

			last_tile_pos.y = tile_pos.y;
		}

		// If we collided with a sloped tile, we have to check one more step
		// forward because we may still collide with another tile at an
		// earlier .length point. For fully solid tiles (id: 1), we can
		// return here.
		if (res.tile > 0 && (res.tile == 1 || extra_step_for_slope)) {
			return res;
		}
		extra_step_for_slope = true;
	}

	return res;	
}

static inline void check_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res) {
	uint32_t tile = map_tile_at(map, tile_pos);
	if (tile == 0) {
		return;
	}
	else if (tile == 1) {
		resolve_full_tile(map, pos, vel, size, tile_pos, res);
	}
	else {
		resolve_sloped_tile(map, pos, vel, size, tile_pos, tile, res);
	}
}

static void resolve_full_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, trace_t *res) {
	// The minimum resulting x or y position in case of a collision. Only
	// the x or y coordinate is correct - depending on if we enter the tile
	// horizontaly or vertically. We will recalculate the wrong one again.

	vec2_t rp = vec2_add(
		vec2_from_vec2i(vec2i_muli(tile_pos, map->tile_size)),
		vec2(
			(vel.x > 0 ? -size.x : map->tile_size),
			(vel.y > 0 ? -size.y : map->tile_size)
		)
	);

	float length = 1;

	// If we don't move in Y direction, or we do move in X and the the tile 
	// corners's cross product with the movement vector has the correct sign, 
	// this is a horizontal collision, otherwise it's vertical.
	// float sign = vec2_cross(vel, vec2_sub(rp, pos)) * vel.x * vel.y;
	float sign = (vel.x * (rp.y - pos.y) - vel.y * (rp.x - pos.x)) * vel.x * vel.y;

	if (sign < 0 || vel.y == 0) {
		// Horizontal collison (x direction, left or right edge)
		length = fabsf((pos.x - rp.x) / vel.x);
	 	if (length > res->length) {
	 		return;
	 	};
		
		rp.y = pos.y + length * vel.y;
		res->normal = vec2((vel.x > 0 ? -1 : 1), 0);
	}
	else {
		// Vertical collision (y direction, top or bottom edge)
		length = fabsf((pos.y - rp.y) / vel.y);
		if (length > res->length) {
			return;
		};
		
		rp.x = pos.x + length * vel.x;
		res->normal = vec2(0, (vel.y > 0 ? -1 : 1));
	}

	res->tile = 1;
	res->tile_pos = tile_pos;
	res->length = length;
	res->pos = rp;
}

static void resolve_sloped_tile(map_t *map, vec2_t pos, vec2_t vel, vec2_t size, vec2i_t tile_pos, uint32_t tile, trace_t *res) {
	if (tile < 2 || tile >= len(slope_definitions)) {
		return;
	}

	const slope_def_t *slope = &slope_definitions[tile];

	// Transform the slope line's starting point (s) and line's direction (d) 
	// into world space coordinates.
	vec2_t tile_pos_px = vec2_mulf(vec2_from_vec2i(tile_pos), map->tile_size);

	vec2_t ss = vec2_mulf(slope->start, map->tile_size);
	vec2_t sd = vec2_mulf(slope->dir, map->tile_size);
	vec2_t local_pos = vec2_sub(pos, tile_pos_px);


	// Do a line vs. line collision with the object's velocity and the slope 
	// itself. This still has problems with precision: When we're moving very
	// slowly along the slope, we might slip behind it.
	// FIXME: maybe the better approach would be to treat every sloped tile as
	// a triangle defined by 3 infinite lines. We could quickly check if the
	// point is within it - but we would still need to determine from which side
	// we're moving into the triangle.

	const float epsilon = 0.001;
	float determinant = vec2_cross(vel, sd);

	if (determinant < -epsilon) {
		vec2_t corner = vec2_add(
			vec2_sub(local_pos, ss),
			vec2(sd.y < 0 ? size.x : 0, sd.x > 0 ? size.y : 0)
		);

		float point_at_slope = vec2_cross(vel, corner) / determinant;
		float point_at_vel = vec2_cross(sd, corner) / determinant;
		
		// Are we in front of the slope and moving into it?
		if (
			point_at_vel > -epsilon &&
			point_at_vel < 1 + epsilon &&
			point_at_slope > -epsilon &&
			point_at_slope < 1 + epsilon
		) {
			// Is this an earlier point than one that we already collided with?
			if (point_at_vel <= res->length) {
				res->tile = tile;
				res->tile_pos = tile_pos;
				res->length = point_at_vel;
				res->normal = slope->normal;
				res->pos = vec2_add(pos, vec2_mulf(vel, point_at_vel));
			}
			return;
		}
	}
	// Is this a non-solid (one-way) tile and we're coming from the wrong side?
	if (!slope->solid && (determinant > 0 || sd.x * sd.y != 0)) {
		return;
	}

	

	// We did not collide with the slope itself, but we still have to check
	// if we collide with the slope's corners or the remaining sides of the
	// tile.

	// Figure out the potential collision points for a horizontal or
	// vertical collision and calculate the min and max coords that will
	// still collide with the tile.

	vec2_t rp;
	vec2_t min;
	vec2_t max;
	float length = 1;

	if (sd.y >= 0) {
		// left tile edge
		min.x = -size.x - epsilon;

		// left or right slope corner?
		max.x = (vel.y > 0 ? ss.x : ss.x + sd.x) - epsilon;
		rp.x = vel.x > 0 ? min.x : max(ss.x, ss.x + sd.x);
	}
	else {
		// left or right slope corner?
		min.x = (vel.y > 0 ? ss.x + sd.x : ss.x) - size.x + epsilon;

		// right tile edge
		max.x = map->tile_size + epsilon; 
		rp.x = vel.x > 0 ? min(ss.x, ss.x + sd.x) - size.x : max.x;
	}

	if (sd.x > 0) {
		// top or bottom slope corner?
		min.y = (vel.x > 0 ? ss.y : ss.y + sd.y) - size.y + epsilon;

		// bottom tile edge
		max.y = map->tile_size + epsilon;
		rp.y = vel.y > 0 ? min(ss.y, ss.y + sd.y) - size.y : max.y;
	}
	else {
		// top tile edge
		min.y = -size.y - epsilon;

		// top or bottom slope corner?
		max.y = (vel.x > 0 ? ss.y + sd.y : ss.y) - epsilon;
		rp.y = vel.y > 0 ? min.y : max(ss.y, ss.y + sd.y);
	}


	// Figure out if this is a horizontal or vertical collision. This
	// step is similar to what we do with full tile collisions.

	float sign = vec2_cross(vel, vec2_sub(rp, local_pos)) * vel.x * vel.y;
	if (sign < 0 || vel.y == 0) {
		// Horizontal collision (x direction, left or right edge)
		length = fabsf((local_pos.x - rp.x) / vel.x);
		rp.y = local_pos.y + length * vel.y;
		
		if (
			rp.y >= max.y || rp.y <= min.y ||
			length > res->length ||
			(!slope->solid && sd.y == 0)
		) {
			return;
		}

		res->normal.x = (vel.x > 0 ? -1 : 1);
		res->normal.y = 0;
	}
	else {
		// Vertical collision (y direction, top or bottom edge)			
		length = fabsf((local_pos.y - rp.y) / vel.y);
		rp.x = local_pos.x + length * vel.x;

		if (
			rp.x >= max.x || rp.x <= min.x ||
			length > res->length ||
			(!slope->solid && sd.x == 0)
		) {
			return;
		}

		res->normal.x = 0;
		res->normal.y = (vel.y > 0 ? -1 : 1);
	}

	res->tile = tile;
	res->tile_pos = tile_pos;
	res->length = length;
	res->pos = vec2_add(rp, tile_pos_px);
}
