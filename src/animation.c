#include <string.h>

#include "animation.h"
#include "alloc.h"
#include "render.h"
#include "engine.h"

anim_def_t *anim_def_with_len(image_t *sheet, vec2i_t frame_size, float frame_time, uint16_t *sequence, uint16_t sequence_len) {
	error_if(engine_is_running(), "Cannot create anim_def during gameplay");

	bool loop = true;
	error_if(sequence_len == 0, "Animation has empty sequence");
	for (int i = 0; i < sequence_len; i++) {
		if (sequence[i] == ANIM_STOP) {
			error_if(i == 0 || i != sequence_len - 1, "Animation can only stop on last frame");
			sequence_len = i-1;
			loop = false;
			break;
		}
	}

	anim_def_t *def = bump_alloc(sizeof(anim_def_t) + sizeof(uint16_t) * sequence_len);
	def->sheet = sheet;
	def->frame_time = frame_time;
	def->inv_total_time = 1.0 / (sequence_len * frame_time);
	def->frame_size = frame_size;
	def->loop = loop;
	def->sequence_len = sequence_len;

	memcpy(def->sequence, sequence, sequence_len * sizeof(uint16_t));
	return def;
}

void anim_rewind(anim_t *anim) {
	anim->start_time = engine.time;
}

void anim_goto(anim_t *anim, int frame) {
	anim->start_time = engine.time + frame * anim->def->frame_time;
}

uint32_t anim_looped(anim_t *anim) {
	double diff = engine.time - anim->start_time;
	return (uint32_t)(diff * anim->def->inv_total_time);
}

void anim_goto_rand(anim_t *anim) {
	anim_goto(anim, rand_int(0, anim->def->sequence_len-1));
}

void anim_draw(anim_t *anim, vec2_t pos) {
	anim_def_t *def = anim->def;
	vec2i_t rs = render_size();
	if (
		pos.x > rs.x || pos.y > rs.y ||
		pos.x + def->frame_size.x < 0 || pos.y + def->frame_size.y < 0 ||
		anim->color.a <= 0
	) {
		return;
	}

	double diff = max(0, engine.time - anim->start_time);
	double looped = diff * def->inv_total_time;

	int frame = !def->loop && looped >= 1
		? def->sequence_len - 1
		: (looped - (int)looped) * def->sequence_len;
	int tile = def->sequence[frame] + anim->tile_offset;

	if (anim->rotation == 0) {
		image_draw_tile_ex(
			def->sheet, tile, def->frame_size, pos, 
			anim->flip_x, anim->flip_y, anim->color
		);
	}
	else {
		render_push();
		render_translate(vec2_add(pos, def->pivot));
		render_rotate(anim->rotation);
		image_draw_tile_ex(
			def->sheet, tile, def->frame_size, vec2_mulf(def->pivot, -1), 
			anim->flip_x, anim->flip_y, anim->color
		);
		render_pop();
	}
}
