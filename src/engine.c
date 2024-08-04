#include "engine.h"
#include "input.h"
#include "render.h"
#include "entity.h"
#include "platform.h"
#include "alloc.h"
#include "utils.h"
#include "image.h"
#include "sound.h"

engine_t engine = {
	.time_real = 0,
	.time_scale = 1.0,
	.time = 0,
	.tick = 0,
	.frame = 0,
	.collision_map = NULL,
	.gravity = 1.0,
};


static scene_t *scene = NULL;
static scene_t *scene_next = NULL;

static texture_mark_t init_textures_mark;
static image_mark_t init_images_mark;
static bump_mark_t init_bump_mark;
static sound_mark_t init_sounds_mark;

static bool is_running = false;

extern void main_init(void);
extern void main_cleanup(void);

void engine_init(void) {
	engine.time_real = platform_now();
	render_init(platform_screen_size());
	sound_init(platform_samplerate());
	platform_set_audio_mix_cb(sound_mix_stereo);
	input_init();
	entities_init();
	main_init();

	init_bump_mark = bump_mark();
	init_images_mark = images_mark();
	init_sounds_mark = sound_mark();
	init_textures_mark = textures_mark();
}

void engine_cleanup(void) {
	entities_cleanup();
	main_cleanup();
	input_cleanup();
	sound_cleanup();
	render_cleanup();
}

void engine_load_level(char *json_path) {
	json_t *json = platform_load_asset_json(json_path);
	error_if(!json, "Could not load level json at %s", json_path);

	entities_reset();
	engine.background_maps_len = 0;
	engine.collision_map = NULL;

	json_t *maps = json_value_for_key(json, "maps");
	for (int i = 0; maps && i < maps->len; i++) {
		json_t *map_def = json_value_at(maps, i);
		char *name = json_string(json_value_for_key(map_def, "name"));
		map_t *map = map_from_json(map_def);

		if (str_equals(name, "collision")) {
			engine_set_collision_map(map);
		}
		else {
			engine_add_background_map(map);
		}
	}

	
	json_t *entities = json_value_for_key(json, "entities");

	// Remember all entities with settings; we want to apply these settings
	// only after all entities have been spawned.
	// FIXME: we do this on the stack. Should maybe use the temp alloc instead.
	struct { entity_t *entity; json_t *settings; } entity_settings[entities->len];
	int entity_settings_len = 0;

	for (int i = 0; entities && i < entities->len; i++) {
		json_t *def = json_value_at(entities, i);
		
		char *type_name = json_string(json_value_for_key(def, "type"));
		error_if(!type_name, "Entity has no type");
		
		entity_type_t type = entity_type_by_name(type_name);
		error_if(!type, "Unknown entity type %s", type_name);

		vec2_t pos = {
			json_number(json_value_for_key(def, "x")),
			json_number(json_value_for_key(def, "y"))
		};

		entity_t *ent = entity_spawn(type, pos);
		json_t *settings = json_value_for_key(def, "settings");
		if (ent && settings && settings->type == JSON_OBJECT) {

			// Copy name, if we have one
			json_t *name = json_value_for_key(settings, "name");
			if (name && name->type == JSON_STRING) {
				ent->name = bump_alloc(name->len + 1);
				strcpy(ent->name, name->string);
			}

			entity_settings[entity_settings_len].entity = ent;
			entity_settings[entity_settings_len].settings = settings;
			entity_settings_len++;
		}
	}

	for (int i = 0; i < entity_settings_len; i++) {
		entity_settings(entity_settings[i].entity, entity_settings[i].settings);
	}
	temp_free(json);
}

void engine_add_background_map(map_t *map) {
	error_if(engine.background_maps_len >= ENGINE_MAX_BACKGROUND_MAPS, "BACKGROUND_MAPS_MAX reached");
	engine.background_maps[engine.background_maps_len++] = map;
}

void engine_set_collision_map(map_t *map) {
	engine.collision_map = map;
}

void engine_set_scene(scene_t *scene) {
	scene_next = scene;
}

void engine_update(void) {
	double time_frame_start = platform_now();

	// Do we want to switch scenes?
	if (scene_next) {
		is_running = false;
		if (scene && scene->cleanup) {
			scene->cleanup();
		}

		textures_reset(init_textures_mark);
		images_reset(init_images_mark);
		sound_reset(init_sounds_mark);
		bump_reset(init_bump_mark);
		entities_reset();

		engine.background_maps_len = 0;
		engine.collision_map = NULL;
		engine.time = 0;
		engine.frame = 0;
		engine.viewport = vec2(0, 0);

		scene = scene_next;
		if (scene->init) {
			scene->init();
		}
		scene_next = NULL;
	}
	is_running = true;

	error_if(scene == NULL, "No scene set");

	double time_real_now = platform_now();
	double real_delta = time_real_now - engine.time_real;
	engine.time_real = time_real_now;
	engine.tick = min(real_delta * engine.time_scale, ENGINE_MAX_TICK);
	engine.time += engine.tick;
	engine.frame++;

	alloc_pool() {
		if (scene->update) {
			scene->update();
		}
		else {
			scene_base_update();
		}

		engine.perf.update = platform_now() - time_real_now;
		
		render_frame_prepare();

		if (scene->draw) {
			scene->draw();
		}
		else {
			scene_base_draw();
		}
		
		render_frame_end();
		engine.perf.draw = (platform_now() - time_real_now) - engine.perf.update;
	}

	input_clear();
	temp_alloc_check();

	engine.perf.draw_calls = render_draw_calls();
	engine.perf.total =  platform_now() - time_frame_start;
}

bool engine_is_running(void) {
	return is_running;
}

void engine_resize(vec2i_t size) {
	render_resize(size);
}


void scene_base_update(void) {
	entities_update();
}

void scene_base_draw(void) {
	vec2_t px_viewport = render_snap_px(engine.viewport);
	
	// Background maps
	for (int i = 0; i < engine.background_maps_len; i++) {
		if (!engine.background_maps[i]->foreground) {
			map_draw(engine.background_maps[i], px_viewport);
		}
	}

	entities_draw(px_viewport);

	// Foreground maps
	for (int i = 0; i < engine.background_maps_len; i++) {
		if (engine.background_maps[i]->foreground) {
			map_draw(engine.background_maps[i], px_viewport);
		}
	}
}