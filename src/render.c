#include <math.h>
#include "render.h"
#include "utils.h"


#if defined(RENDER_GL)
	#include "render_gl.c"
#elif defined(RENDER_SOFTWARE)
	#include "render_software.c"
#else
	#error "No renderer specified. #define RENDER_GL or RENDER_SOFTWARE"
#endif


static uint32_t draw_calls = 0;
static float screen_scale;
static float inv_screen_scale;
static vec2i_t screen_size;
static vec2i_t logical_size;

static mat3_t transform_stack[RENDER_TRANSFORM_STACK_SIZE];
static uint32_t transform_stack_index = 0;

void render_init(vec2i_t avaiable_size) {
	render_backend_init();
	render_resize(avaiable_size);
	transform_stack[0] = mat3_identity();
}

void render_cleanup(void) {
	render_backend_cleanup();
}

uint32_t render_draw_calls(void) {
	uint32_t r = draw_calls;
	draw_calls = 0;
	return r;
}

void render_resize(vec2i_t avaiable_size) {
	// Determine Zoom
	if (RENDER_SCALE_MODE == RENDER_SCALE_NONE) {
		screen_scale = 1;
	}
	else {
		screen_scale = min(
			avaiable_size.x / (float)RENDER_WIDTH,
			avaiable_size.y / (float)RENDER_HEIGHT
		);

		if (RENDER_SCALE_MODE == RENDER_SCALE_DISCRETE) {
			screen_scale = max(floor(screen_scale), 0.5);
		}
	}

	// Determine size
	if (RENDER_RESIZE_MODE & RENDER_RESIZE_WIDTH) {
		screen_size.x = max(avaiable_size.x, RENDER_WIDTH);
	}
	else {
		screen_size.x = RENDER_WIDTH * screen_scale;
	}

	if (RENDER_RESIZE_MODE & RENDER_RESIZE_HEIGHT) {
		screen_size.y = max(avaiable_size.y, RENDER_HEIGHT);
	}
	else {
		screen_size.y = RENDER_HEIGHT * screen_scale;
	}

	logical_size.x = ceil(screen_size.x / screen_scale);
	logical_size.y = ceil(screen_size.y / screen_scale);
	inv_screen_scale = 1.0 / screen_scale;
	render_set_screen(screen_size);
}

vec2i_t render_size(void) {
	return logical_size;
}

void render_push(void) {
	error_if(transform_stack_index >= RENDER_TRANSFORM_STACK_SIZE-1, "Max transform stack size (%d) reached", RENDER_TRANSFORM_STACK_SIZE);
	transform_stack[transform_stack_index+1] = transform_stack[transform_stack_index];
	transform_stack_index++;
}

void render_pop(void) {
	error_if(transform_stack_index == 0, "Cannot pop from empty transform stack");
	transform_stack_index--;
}

void render_translate(vec2_t translate) {
	error_if(transform_stack_index == 0, "Cannot translate initial transform. render_push() first.");
	translate = vec2_mulf(translate, screen_scale);
	mat3_translate(&transform_stack[transform_stack_index], translate);
}

void render_scale(vec2_t scale) {
	error_if(transform_stack_index == 0, "Cannot scale initial transform. render_push() first.");
	mat3_scale(&transform_stack[transform_stack_index], scale);
}

void render_rotate(float rotation) {
	error_if(transform_stack_index == 0, "Cannot rotate initial transform. render_push() first.");
	mat3_rotate(&transform_stack[transform_stack_index], rotation);
}

vec2_t render_snap_px(vec2_t pos) {
	vec2_t sp = vec2_mulf(pos, screen_scale);
	return vec2_mulf(vec2(round(sp.x), round(sp.y)), inv_screen_scale);
}

void render_draw(vec2_t pos, vec2_t size, texture_t texture_handle, vec2_t uv_offset, vec2_t uv_size, rgba_t color) {
	if (
		pos.x > logical_size.x || pos.y > logical_size.y ||
		pos.x + size.x < 0     || pos.y + size.y < 0
	) {
		return;
	}

	pos = vec2_mulf(pos, screen_scale);
	size = vec2_mulf(size, screen_scale);
	draw_calls++;

	quadverts_t q = {
		.vertices = {
			{
				.pos = {pos.x, pos.y},
				.uv = {uv_offset.x , uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y},
				.uv = {uv_offset.x +  uv_size.x, uv_offset.y},
				.color = color
			},
			{
				.pos = {pos.x + size.x, pos.y + size.y},
				.uv = {uv_offset.x + uv_size.x, uv_offset.y + uv_size.y},
				.color = color
			},
			{
				.pos = {pos.x, pos.y + size.y},
				.uv = {uv_offset.x, uv_offset.y + uv_size.y},
				.color = color
			}
		}
	};

	if (transform_stack_index > 0) {
		mat3_t *m = &transform_stack[transform_stack_index];
		for (uint32_t i = 0; i < 4; i++) {
			q.vertices[i].pos = vec2_transform(q.vertices[i].pos, m);
		}
	}

	render_draw_quad(&q, texture_handle);
}
