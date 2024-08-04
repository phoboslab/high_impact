#ifndef HI_RENDER_H
#define HI_RENDER_H

// A renderer is responsible for drawing on the screen. Images, Fonts and
// Animations ulitmately use the render_* functions to be drawn.
// Different renderer backends can be implemented by supporting just a handful
// of functions.

#include "types.h"


// The desired "logical size" or viewport size of the screen. This may be 
// different from the real pixel size. E.g. you can have a window with size of 
// 640x480 and a render size of 320x240. Note that, depending on the RESIZE_MODE 
// this logical size may also change when you resize the window.
#if !defined(RENDER_WIDTH) || !defined(RENDER_HEIGHT)
	#define RENDER_WIDTH 1280
	#define RENDER_HEIGHT 720
#endif

// The scale mode determines if and how the logical size will be scaled up when
// the window is larger than the render size. Note that the desired aspect ratio
// will be maintained (depending on RESIZE_MODE).
// RENDER_RESIZE_NONE    - no scaling
// RENDER_SCALE_DISCRETE - scale in integer steps for perfect pixel scaling
// RENDER_SCALE_EXACT    - scale exactly to the window size
#if !defined(RENDER_SCALE_MODE)
	#define RENDER_SCALE_MODE RENDER_SCALE_DISCRETE
#endif

// The resize mode determines how the logical size changes to adapt to the
// available window size.
// RENDER_RESIZE_NONE    - don't resize
// RENDER_RESIZE_WIDTH   - resize only width; keep height fixed at RENDER_HEIGHT
// RENDER_RESIZE_HEIGHT  - resize only height; keep width fixed at RENDER_WIDTH
// RENDER_RESIZE_ANY     - resize width and height to fill the window
#if !defined(RENDER_RESIZE_MODE)
	#define RENDER_RESIZE_MODE RENDER_RESIZE_ANY
#endif

// The maximum size of the transform stack, when using render_push()
#if !defined(RENDER_TRANSFORM_STACK_SIZE)
	#define RENDER_TRANSFORM_STACK_SIZE 16
#endif

// The maximum number of textures to be loaded at a time
#if !defined(RENDER_TEXTURES_MAX)
	#define RENDER_TEXTURES_MAX 1024
#endif

typedef enum {
	RENDER_SCALE_NONE,
	RENDER_SCALE_DISCRETE,
	RENDER_SCALE_EXACT
} render_scale_mode_t;

typedef enum  {
	RENDER_RESIZE_NONE    = 0,
	RENDER_RESIZE_WIDTH   = 1,
	RENDER_RESIZE_HEIGHT  = 2,
	RENDER_RESIZE_ANY     = 3,
} render_resize_mode_t;

typedef enum {
	RENDER_BLEND_NORMAL,
	RENDER_BLEND_LIGHTER
} render_blend_mode_t;

typedef enum {
	RENDER_POST_NONE,
	RENDER_POST_CRT,
	RENDER_POST_MAX,
} render_post_effect_t;

typedef struct {
	vec2_t pos;
	vec2_t uv;
	rgba_t color;
} vertex_t;

typedef struct {
	vertex_t vertices[4];
} quadverts_t;

typedef struct { uint32_t index; } texture_mark_t;
typedef struct { uint32_t index; } texture_t;
extern texture_t RENDER_NO_TEXTURE;


// Called by the platform
void render_init(vec2i_t screen_size);
void render_cleanup(void);

// Return the number of draw calls for the previous frame
uint32_t render_draw_calls(void);

// Resize the logical size according to the available window size and the scale
// and resize mode
void render_resize(vec2i_t avaiable_size);

// Return the logical size
vec2i_t render_size(void);

// Push the transform stack
void render_push(void);

// Pop the transform stack
void render_pop(void);

// Translate; can only be called if stack was pushed at least once
void render_translate(vec2_t translate);

// Scale; can only be called if stack was pushed at least once
void render_scale(vec2_t scale);

// Rortate; can only be called if stack was pushed at least once
void render_rotate(float rotation);

// Returns a logical position, snapped to real screen pixels
vec2_t render_snap_px(vec2_t pos);

// Draws a rect with the given logical position, size, texture, uv-coords and
// color, transformed by the current transform stack
void render_draw(vec2_t pos, vec2_t size, texture_t texture_handle, vec2_t uv_offset, vec2_t uv_size, rgba_t color);



// The following functions must be implemented by render backend ---------------

void render_backend_init(void);
void render_backend_cleanup(void);

void render_set_screen(vec2i_t size);
void render_set_blend_mode(render_blend_mode_t mode);
void render_set_post_effect(render_post_effect_t post);
void render_frame_prepare(void);
void render_frame_end(void);
void render_draw_quad(quadverts_t *quad, texture_t texture_handle);

texture_mark_t textures_mark(void);
void textures_reset(texture_mark_t mark);
texture_t texture_create(vec2i_t size, rgba_t *pixels);
void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels);

#endif
