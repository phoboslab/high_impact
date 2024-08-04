#ifndef HI_MAP_H
#define HI_MAP_H

// A map consists of an array of tile indices and can be drawn or used for
// collision testing with trace(). Maps can be loaded from a json_t or created
// with a data array.

#include "types.h"
#include "../libs/pl_json.h"
#include "image.h"
#include "animation.h"

typedef struct map_anim_def_t map_anim_def_t;

typedef struct {
	// The size of the map in tiles
	vec2i_t size;

	// The size of a tile of this map
	uint16_t tile_size;

	// The name of the map. For collision maps this is usually "collision".
	// Background maps may have any name.
	char name[16];

	// The "distance" of the map when drawing at a certain offset. Maps that
	// have a higher distance move slower. Default 1.
	float distance;

	// Whether the map repeats indefinitely when drawing
	bool repeat;

	// Whether to draw this map in fround of all entities
	bool foreground;

	// The tileset image to use when drawing. Might be NULL for collision maps
	image_t *tileset;

	// Animations for certain tiles when drawing. Use map_set_anim() to add
	// animations.
	map_anim_def_t **anims;

	// The tile indices with a length of size.x * size.y
	uint16_t *data;

	// The highest tile index in that map; used internally.
	uint16_t max_tile;
} map_t;

// Create a map with the given data. If data is not NULL, it must be least 
// size.x * size.y elements long. The data is _not_ copied. If data is NULL,
// an array of sufficent length will be allocated.
map_t *map_with_data(uint16_t tile_size, vec2i_t size, uint16_t *data);

// Load a map from a json_t. The json_t must have the following layout.
// Note that tile indices have a bias of +1. I.e. index 0 will not draw anything
// and represent a blank tile. Index 1 will draw the 0th tile from the tileset.
/*
{
	"name": "background",
	"width": 4,
	"height": 2,
	"tilesetName": "assets/tiles/biolab.qoi",
	"repeat": true,
	"distance": 1.0,
	"tilesize": 8,
	"foreground": false,
	"data": [
		[0,1,2,3],
		[3,2,1,0],
	]
}
*/
map_t *map_from_json(json_t *def);

// Set the frame time and animation sequence for a particular tile. You can
// only do this in your scene_init()
#define map_set_anim(MAP, TILE, FRAME_TIME, ...) \
	map_set_anim_with_len(MAP, TILE, FRAME_TIME, (uint16_t[])__VA_ARGS__, len((uint16_t[])__VA_ARGS__))
void map_set_anim_with_len(map_t *map, uint16_t tile, float frame_time, uint16_t *sequence, uint16_t sequence_len);

// Return the tile index at the tile position. Will return 0 when out of bounds
int map_tile_at(map_t *map, vec2i_t tile_pos);

// Return the tile index at the pixel position. Will return 0 when out of bounds
int map_tile_at_px(map_t *map, vec2_t px_pos);

// Draw the map at the given offset. This will take the distance into account.
void map_draw(map_t *map, vec2_t offset);

#endif
