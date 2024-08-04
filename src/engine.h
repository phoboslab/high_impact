#ifndef HI_ENGINE_H
#define HI_ENGINE_H

// The engine is the main wrapper around your. For every frame, it will update
// your scene, update all entities and draw the whole frame.

// The engine takes care of timekeeping, a number background maps, a collision 
// map some more global state. There's only one engine_t instance in high_impact
// and it's globally available at `engine`

#include "types.h"
#include "map.h"

#include "../libs/pl_json.h"


// The maximum difference in seconds from one frame to the next. If the 
// difference  is larger than this, the game will slow down instead of having
// imprecise large time steps.
#if !defined(ENGINE_MAX_TICK)
	#define ENGINE_MAX_TICK 0.1
#endif

// The maximum number of background maps
#if !defined(ENGINE_MAX_BACKGROUND_MAPS)
	#define ENGINE_MAX_BACKGROUND_MAPS 4
#endif


// Every scene in your game must provide a scene_t that specifies it's entry
// functions.
typedef struct {
	// Called once when the scene is set. Use it to load resources and 
	// instaiate your initial entities
	void (*init)(void);

	// Called once per frame. Uss this to update logic specific to your game.
	// If you use this function, you probably want to call scene_base_update()
	// in it somewhere.
	void (*update)(void);

	// Called once per frame. Use this to e.g. draw a background or hud.
	// If you use this function, you probably want to call scene_base_draw()
	// in it somewhere.
	void (*draw)(void);

	// Called once before the next scene is set or the game ends
	void (*cleanup)(void);
} scene_t;

typedef struct {
	// The real time in seconds since program start
	double time_real;

	// The game time in seconds since scene start
	double time;

	// A global multiplier for how fast game time should advance. Default: 1.0
	double time_scale;

	// The time difference in seconds from the last frame to the current. 
	// Typically 0.01666 (assuming 60hz)
	double tick;

	// The frame number in this current scene. Increases by 1 for every frame.
	uint64_t frame;

	// The map to use for entity vs. world collisions. Reset for each scene.
	// Use engine_set_collision_map() to set it.
	map_t *collision_map;

	// The maps to draw. Reset for each scene. Use engine_add_background_map()
	// to add.
	map_t *background_maps[ENGINE_MAX_BACKGROUND_MAPS];
	uint32_t background_maps_len;

	// A global multiplier that affects the gravity of all entities. This only
	// makes sense for side view games. For a top-down game you'd want to have 
	// it at 0.0. Default: 1.0
	float gravity;

	// The top left corner of the viewport. Internally just an offset when 
	// drawing background_maps and entities.
	vec2_t viewport;

	// Various infos about the last frame
	struct {
		int entities;
		int checks;
		int draw_calls;
		float update;
		float draw;
		float total;
	} perf;
} engine_t;

extern engine_t engine;

// Makes the scene_the current scene. This calls scene->cleanup() on the old
// scene and scene->init() on the new one. The actual swap of scenes happens
// at the beginning of the next frame, so it's ok to call engine_set_scene()
// from the middle of a frame.
// Your main_init() function must call engine_set_scene() to set first scene.
void engine_set_scene(scene_t *scene);

// Load a level (background maps, collision map and entities) from a json path.
// This should only be called from within your scenes init() function.
void engine_load_level(char *json_path);

// Add a background map; typically done through engine_load_level()
void engine_add_background_map(map_t *map);

// Set the collision map; typically done through engine_load_level()
void engine_set_collision_map(map_t *map);

// Whether the game is running or we are in a loading phase (i.e. when swapping
// scenes)
bool engine_is_running(void);

// Update all entities
void scene_base_update(void);

// Draw all background maps and entities
void scene_base_draw(void);

// The following functions are automatically called by the platform. No need
// to call yourself.
void engine_init(void);
void engine_update(void);
void engine_cleanup(void);
void engine_resize(vec2i_t size);


#endif
