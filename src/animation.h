#ifndef HI_ANIMATION_H
#define HI_ANIMATION_H

// Animations use an image() as the animation sheet and divide this sheet into
// several animation frames according to `frame_size`. A sequence of frame 
// numbers and the time per frame is then used to decide which frame to draw.

// Animations are split in two parts: the animation definition (`anim_def_t`)
// and an actual instance of the animation (`anim_t`). anim_def_t contain the 
// number of frames, the frame sequence and the time per frame. anim_t holds
// the current state for the animation and decide which frame to draw.

// Each anim_def_t can be shared by any number of anim_t instances.

#include "types.h"
#include "image.h"
#include "engine.h"
#include "utils.h"

typedef struct {
	image_t *sheet;
	vec2i_t frame_size;
	bool loop;
	vec2_t pivot;
	float frame_time;
	float inv_total_time;
	uint16_t sequence_len;
	uint16_t sequence[];
} anim_def_t;

typedef struct {
	anim_def_t *def;
	double start_time;
	uint16_t tile_offset;
	bool flip_x;
	bool flip_y;
	float rotation;
	rgba_t color;
} anim_t;

// Create an anim_def with the given sheet, frame_size, frame_time and sequence.
// This macro automatically calculates the length of the sequence, but only 
// works if the sequence is provided as a literal.
// E.g.: anim_def(sheet, vec2i(16, 8), 0.5, {0,1,2,3,4});
#define ANIM_STOP 0xffff
#define anim_def(SHEET, FRAME_SIZE, FRAME_TIME, ...) \
	anim_def_with_len(SHEET, FRAME_SIZE, FRAME_TIME, (uint16_t[])__VA_ARGS__, len((uint16_t[])__VA_ARGS__))


// Create an anim_def with the given sheet, frame_size, sequence and sequence_len
anim_def_t *anim_def_with_len(image_t *sheet, vec2i_t frame_size, float frame_time, uint16_t *sequence, uint16_t sequence_len);

// Create an anim_t instance with the given anim_def
#define anim(ANIM_DEF)(anim_t){.def = ANIM_DEF, .color = rgba_white(), .start_time = engine.time}

// Rewind the animation to the first frame of the sequence
void anim_rewind(anim_t *anim);

// Goto to the nth index of the sequence
void anim_goto(anim_t *anim, int frame);

// Goto a random frame of the sequence
void anim_goto_rand(anim_t *anim);

// Return the number of times this animation has played through
uint32_t anim_looped(anim_t *anim);

// Draw the animation at the given pos
void anim_draw(anim_t *anim, vec2_t pos);

#endif
