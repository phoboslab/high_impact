#include "image.h"
#include "alloc.h"
#include "render.h"
#include "utils.h"
#include "engine.h"
#include "platform.h"

#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#define QOI_MALLOC temp_alloc
#define QOI_FREE temp_free
#include "../libs/qoi.h"

struct image_t {
	vec2i_t size;
	texture_t texture;
};

static image_t images[IMAGE_MAX_SOURCES] = {};
static char *image_paths[IMAGE_MAX_SOURCES] = {};
static uint32_t images_len = 0;
static char *image_internal_path = "__internal";


image_mark_t images_mark(void) {
	return (image_mark_t){.index = images_len};
}

void images_reset(image_mark_t mark) {
	images_len = mark.index;
}

image_t *image_with_pixels(vec2i_t size, rgba_t *pixels) {
	error_if(images_len >= IMAGE_MAX_SOURCES, "Max images (%d) reached", IMAGE_MAX_SOURCES);
	error_if(engine_is_running(), "Cannot create image during gameplay");

	image_paths[images_len] = image_internal_path;

	image_t *img = &images[images_len];
	img->size = size;
	img->texture = texture_create(size, pixels);

	images_len++;
	return img;
}

image_t *image(char *path) {
	for (uint32_t i = 0; i < images_len; i++) {
		if (str_equals(path, image_paths[i])) {
			return &images[i];
		}
	}

	error_if(images_len >= IMAGE_MAX_SOURCES, "Max images (%d) reached", IMAGE_MAX_SOURCES);
	error_if(engine_is_running(), "Cannot load image during gameplay");

	image_paths[images_len] = bump_alloc(strlen(path)+1);
	strcpy(image_paths[images_len], path);


	uint32_t file_size;
	uint8_t *data = platform_load_asset(path, &file_size);
	error_if(data == NULL, "Failed to load image %s", path);

	qoi_desc desc;
	rgba_t *pixels = qoi_decode(data, file_size, &desc, 4);
	error_if(pixels == NULL, "Failed to decode image: %s", path);
	temp_free(data);

	vec2i_t size = vec2i(desc.width, desc.height);
	image_t *img = &images[images_len];
	img->size = size;
	img->texture = texture_create(size, pixels);

	images_len++;

	temp_free(pixels);
	return img;
}

vec2i_t image_size(image_t *img) {
	return img->size;
}

void image_draw(image_t *img, vec2_t pos) {
	vec2_t size = vec2_from_vec2i(img->size);
	render_draw(pos, size, img->texture, vec2(0, 0), size, rgba_white());
}

void image_draw_ex(image_t *img, vec2_t src_pos, vec2_t src_size, vec2_t dst_pos, vec2_t dst_size, rgba_t color) {
	render_draw(dst_pos, dst_size, img->texture, src_pos, src_size, color);
}

void image_draw_tile(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos) {
	image_draw_tile_ex(img, tile, tile_size, dst_pos, false, false, rgba_white());
}

void image_draw_tile_ex(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos, bool flip_x, bool flip_y, rgba_t color) {
	vec2_t src_pos = vec2(
		(tile * tile_size.x) % img->size.x,
		((tile * tile_size.x) / img->size.x) * tile_size.y
	);
	vec2_t src_size = vec2(tile_size.x, tile_size.y);
	vec2_t dst_size = src_size;

	if (flip_x) {
		src_pos.x = src_pos.x + tile_size.x;
		src_size.x = -tile_size.x;
	}
	if (flip_y) {
		src_pos.y = src_pos.y + tile_size.y;
		src_size.y = -tile_size.y;
	}
	render_draw(dst_pos, dst_size, img->texture, src_pos, src_size, color);
}
