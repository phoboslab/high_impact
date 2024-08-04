#include "camera.h"
#include "entity.h"
#include "engine.h"
#include "render.h"

static vec2_t camera_viewport_target(camera_t *cam) {
	vec2_t screen_size = vec2_from_vec2i(render_size());
	vec2_t screen_center = vec2_mulf(screen_size, 0.5);
	vec2_t viewport_target = vec2_add(vec2_sub(cam->pos, screen_center), cam->offset);

	if (engine.collision_map) {
		vec2_t bounds = vec2_from_vec2i(vec2i_muli(engine.collision_map->size, engine.collision_map->tile_size));
		viewport_target.x = clamp(viewport_target.x, 0, bounds.x - screen_size.x);
		viewport_target.y = clamp(viewport_target.y, 0, bounds.y - screen_size.y);
	}
	return viewport_target;
}

void camera_update(camera_t *cam) {
	entity_t *follow = entity_by_ref(cam->follow);
	if (follow) {
		vec2_t size = vec2(
			min(follow->size.x, cam->deadzone.x),
			min(follow->size.y, cam->deadzone.y)
		);

		if (follow->pos.x < cam->deadzone_pos.x) {
			cam->deadzone_pos.x = follow->pos.x;
			cam->look_ahead_target.x = -cam->look_ahead.x;
		}
		else if (follow->pos.x + size.x > cam->deadzone_pos.x + cam->deadzone.x) {
			cam->deadzone_pos.x = follow->pos.x + size.x - cam->deadzone.x;
			cam->look_ahead_target.x = cam->look_ahead.x;
		}

		if (follow->pos.y < cam->deadzone_pos.y) {
			cam->deadzone_pos.y = follow->pos.y;
			cam->look_ahead_target.y = -cam->look_ahead.y;
		}
		else if (follow->pos.y + size.y > cam->deadzone_pos.y + cam->deadzone.y) {
			cam->deadzone_pos.y = follow->pos.y + size.y - cam->deadzone.y;
			cam->look_ahead_target.y = cam->look_ahead.y;
		}

		if (cam->snap_to_platform && follow->on_ground) {
			cam->deadzone_pos.y = follow->pos.y + follow->size.y - cam->deadzone.y;
		}
		vec2_t deadzone_target = vec2_add(cam->deadzone_pos, vec2_mulf(cam->deadzone, 0.5));
		cam->pos = vec2_add(deadzone_target, cam->look_ahead_target);
	}	

	vec2_t diff = vec2_sub(camera_viewport_target(cam), engine.viewport);
	cam->vel = vec2_mulf(diff, cam->speed);

	if (fabsf(cam->vel.x) + fabsf(cam->vel.y) > cam->min_vel) {
		engine.viewport = vec2_add(engine.viewport, vec2_mulf(cam->vel, engine.tick));
	}
}


void camera_set(camera_t *cam, vec2_t pos) {
	cam->pos = pos;
	engine.viewport = camera_viewport_target(cam);
}

void camera_move(camera_t *cam, vec2_t pos) {
	cam->pos = pos;
}

void camera_follow(camera_t *cam, entity_ref_t follow, bool snap) {
	cam->follow = follow;
	if (snap) {
		camera_update(cam);
		engine.viewport = camera_viewport_target(cam);
	}
}

void camera_unfollow(camera_t *cam) {
	cam->follow = entity_ref_none();
}