#ifndef HI_IMAGE_H
#define HI_IMAGE_H

// Images can be loaded from QOI files or directly created with an array of
// rgba_t pixels. If an image at a certain path is already loaded, calling
// image() with that same path, will return the same image.
// Images can be drawn to the screen in full, just parts of it, or as a "tile" 
// from it.

#include "types.h"

// The maximum number of images we expect to have loaded at one time
#if !defined(IMAGE_MAX_SOURCES)
	#define IMAGE_MAX_SOURCES 1024
#endif

typedef struct image_t image_t;

// Create an image with an array of size.x * size.y pixels
image_t *image_with_pixels(vec2i_t size, rgba_t *pixels);

// Load an image from a QOI file. Calling this function multiple times with the
// same path will return the same, cached image instance.
image_t *image(char *path);

// Return the size of an image
vec2i_t image_size(image_t *img);

// Draw the whole image at pos
void image_draw(image_t *img, vec2_t pos);

// Draw the src_pos, src_size rect of the image to dst_pos with dst_size and a tint color
void image_draw_ex(image_t *img, vec2_t src_pos, vec2_t src_size, vec2_t dst_pos, vec2_t dst_size, rgba_t color);

// Draw a single tile from the image, as subdivided by tile_size
void image_draw_tile(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos);

// Draw a single tile and specify x/y flipping and a tint color
void image_draw_tile_ex(image_t *img, uint32_t tile, vec2i_t tile_size, vec2_t dst_pos, bool flip_x, bool flip_y, rgba_t color);

// Called by the engine to manage image memory
typedef struct { uint32_t index; } image_mark_t;
image_mark_t images_mark(void);
void images_reset(image_mark_t mark);

#endif
