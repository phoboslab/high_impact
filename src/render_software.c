#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"
#include "platform.h"

texture_t RENDER_NO_TEXTURE;

struct {
	vec2i_t size;
	rgba_t *pixels;
} textures[RENDER_TEXTURES_MAX];

uint32_t textures_len = 0;

static rgba_t *screen_buffer;
static int32_t screen_pitch;
static int32_t screen_ppr;
static vec2i_t screen_size;

void render_backend_init(void) {}
void render_backend_cleanup(void) {}

void render_set_screen(vec2i_t size) {
	screen_size = size;
}

void render_set_blend_mode(render_blend_mode_t mode) {
	// TODO
}

void render_set_post_effect(render_post_effect_t post) {
	// TODO
}

void render_frame_prepare(void) {
	screen_buffer = platform_get_screenbuffer(&screen_pitch);
	screen_ppr = screen_pitch / sizeof(rgba_t);

	memset(screen_buffer, 0, screen_size.y * screen_pitch);
}

void render_frame_end(void) {}

void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	// FIXME: this only handles axis aligned quads; rotation/shearing is not
	// supported.

	vertex_t *v = quad->vertices;
	rgba_t color = v[0].color;

	int dx = v[0].pos.x;
	int dy = v[0].pos.y;
	int dw = v[2].pos.x - dx;
	int dh = v[2].pos.y - dy;

	vec2i_t src_size = textures[texture_handle.index].size;
	rgba_t *src_px = textures[texture_handle.index].pixels;

	vec2i_t uv_tl = vec2i_from_vec2(v[0].uv);
	uv_tl.x = clamp(uv_tl.x, 0, src_size.x);
	uv_tl.y = clamp(uv_tl.y, 0, src_size.y);

	vec2i_t uv_br = vec2i_from_vec2(v[2].uv);
	uv_br.x = clamp(uv_br.x, 0, src_size.x);
	uv_br.y = clamp(uv_br.y, 0, src_size.y);

	float sx = uv_tl.x;
	float sy = uv_tl.y;
	float sw = uv_br.x - sx;
	float sh = uv_br.y - sy;

	float sx_inc = sw / dw;
	float sy_inc = sh / dh;

	// Clip to screen
	if (dx < 0) {
		sx += sx_inc * -dx;
		dw += dx;
		dx = 0;
	}
	if (dx + dw >= screen_size.x) {
		dw = screen_size.x - dx;
	}
	if (dy < 0) {
		sy += sy_inc * -dy;
		dh += dy;
		dy = 0;
	}
	if (dy + dh >= screen_size.y) {
		dh = screen_size.y - dy;
	}


	// FIXME: There's probably an underflow in the source data when 
	// sx_inc or sy_inc is negative?!
	int di = dy * screen_ppr + dx;
	for (int y = 0; y < dh; y++, di += screen_ppr - dw) {
		// fudge source index by 0.001 pixels to avoid rounding errors :/
		float si = floor(sy + y * sy_inc) * src_size.x + sx + 0.001;
		for (int x = 0; x < dw; x++, si += sx_inc, di++) {
			screen_buffer[di] = rgba_blend(screen_buffer[di], rgba_mix(src_px[(int)si], color));
		}
	}
}

texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textures_len};
}

void textures_reset(texture_mark_t mark) {
	error_if(mark.index > textures_len, "Invalid texture reset mark %d >= %d", mark.index, textures_len);
	textures_len = mark.index;
}

texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	error_if(textures_len >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");

	textures[textures_len].size = size;
	textures[textures_len].pixels = bump_alloc(sizeof(rgba_t) * size.x * size.y);
	memcpy(textures[textures_len].pixels, pixels, sizeof(rgba_t) * size.x * size.y);

	texture_t texture_handle = {.index = textures_len};
	textures_len++;
	return texture_handle;
}

void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	vec2i_t dst_size = textures[texture_handle.index].size;
	rgba_t *dst_px = textures[texture_handle.index].pixels;
	error_if(dst_size.x < size.x || dst_size.y < size.y, "Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, dst_size.x, dst_size.y);

	int di = 0;
	int si = 0;
	for (int y = 0; y < size.y; y++, di += dst_size.x - size.x) {
		for (int x = 0; x < size.x; x++, si++, di++) {
			dst_px[di] = pixels[si];
		}
	}
}
