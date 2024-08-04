#include "sound.h"
#include "utils.h"
#include "engine.h"
#include "alloc.h"
#include "platform.h"

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO
#include "../libs/qoa.h"


typedef enum {
	SOUND_TYPE_PCM = 1,
	SOUND_TYPE_QOA = 2,
} sound_source_type_t;

typedef struct {
	qoa_desc desc;
	uint32_t data_len;
	uint8_t *data;
	uint32_t pcm_buffer_start;
	int16_t *pcm_buffer;
} sound_source_qoa_t;

struct sound_source_t {
	sound_source_type_t type;
	uint32_t channels;
	uint32_t len;
	uint32_t samplerate;
	union {
		int16_t *pcm_samples;
		sound_source_qoa_t *qoa;
	};
};

typedef struct {
	sound_source_t *source;
	uint16_t id;
	bool is_playing;
	bool is_halted;
	bool is_looping;
	float pan;
	float volume;
	float pitch;
	float sample_pos;
} sound_node_t;



// sound system ----------------------------------------------------------------

static float global_volume = 1;
static float inv_out_samplerate;

static sound_source_t sources[SOUND_MAX_SOURCES];
static uint32_t sources_len = 0;
static char *source_paths[SOUND_MAX_SOURCES] = {};

static sound_node_t sound_nodes[SOUND_MAX_NODES];
static uint32_t nodes_len = 0;
static uint16_t sound_unique_id = 0;


void sound_init(int samplerate) {
	inv_out_samplerate = 1.0 / samplerate;
}

void sound_cleanup(void) {
	// Stop all nodes
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		sound_nodes[i].is_playing = false;
	}
}

sound_mark_t sound_mark(void) {
	return (sound_mark_t){.index = sources_len};
}

void sound_reset(sound_mark_t mark) {
	// Reset all nodes whose sources are invalidated
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		sound_node_t *node = &sound_nodes[i];
		if (node->source - sources >= mark.index) {
			node->id = 0;
			node->is_playing  = false;
			node->is_halted   = false;
			node->is_looping  = false;
		}
	}
	sources_len = mark.index;
}

void sound_halt(void) {
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (sound_nodes[i].is_playing) {
			sound_nodes[i].is_playing = false;
			sound_nodes[i].is_halted  = true;
		}
	}
}

void sound_resume(void) {
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (sound_nodes[i].is_halted) {
			sound_nodes[i].is_playing = true;
			sound_nodes[i].is_halted  = false;
		}
	}
}

float sound_global_volume(void) {
	return global_volume;
}

void sound_set_global_volume(float volume) {
	global_volume = clamp(volume, 0, 1);
}

void sound_mix_stereo(float *dest_samples, uint32_t dest_len) {
	memset(dest_samples, 0, dest_len * sizeof(float));

	// Samples are stored as int16_t; we have to multiply each sample with 
	// the global sound_volume anyway, so do the normalization from int16_t to 
	// float (-1..1) at the same time.
	float volume_normalize = global_volume / 32768.0;

	for (uint32_t n = 0; n < SOUND_MAX_NODES; n++) {
		sound_node_t *node = &sound_nodes[n];

		if (node->is_playing && node->volume > 0) {
			sound_source_t *source = node->source;
			float vol_left = volume_normalize * node->volume * clamp(1.0 - node->pan, 0, 1);
			float vol_right = volume_normalize * node->volume * clamp(1.0 + node->pan, 0, 1);

			// Calculate the pitch by considering the output samplerate. Quality
			// wise, this is not the best way to "resample" the source. FIXME
			float pitch = node->pitch * source->samplerate * inv_out_samplerate;

			sound_source_qoa_t *qoa = NULL;
			int16_t *src_samples = NULL;

			if (source->type == SOUND_TYPE_PCM) {
				src_samples = source->pcm_samples;
			}
			else if (source->type == SOUND_TYPE_QOA) {
				qoa = source->qoa;
				src_samples = source->qoa->pcm_buffer;
			}

			int c = source->channels == 2 ? 1 : 0;

			for (uint32_t di = 0; di < dest_len; di += 2) {
				uint32_t source_index = (uint32_t)node->sample_pos;

				// If this is a compressed source we may have to decode a 
				// different frame for this source index. This will overwrite 
				// the source' internal pcm frame buffer.
				// FIXME: this creates unnecessary decodes when many nodes play
				// the same source?
				if (source->type == SOUND_TYPE_QOA) {
					if (
						source_index < qoa->pcm_buffer_start || 
						source_index >= qoa->pcm_buffer_start + QOA_FRAME_LEN
					) {
						uint32_t frame_index = source_index / QOA_FRAME_LEN;
						uint32_t frame_data_start = qoa_max_frame_size(&qoa->desc) * frame_index;
						uint32_t frame_data_len = qoa->data_len - frame_data_start;
						void *frame_data = qoa->data + frame_data_start;
						uint32_t frame_len;
						qoa_decode_frame(frame_data, frame_data_len, &qoa->desc, src_samples, &frame_len);
						qoa->pcm_buffer_start = frame_index * QOA_FRAME_LEN;
					}
					source_index -= qoa->pcm_buffer_start;
				}
				
				dest_samples[di+0] += src_samples[(source_index << c) + 0] * vol_left;
				dest_samples[di+1] += src_samples[(source_index << c) + c] * vol_right;

				node->sample_pos += pitch;
				if (node->sample_pos >= source->len || node->sample_pos < 0) {
					if (node->is_looping) {
						node->sample_pos = 
							fmod(node->sample_pos, source->len) + 
							(node->sample_pos < 0 ? source->len : 0);
					}
					else {
						node->is_playing = false;
						break;
					}
				}
			}
		}
	}
}


// sound_source ------------------------------------------------------------------

sound_source_t *sound_source(char *path) {
	for (uint32_t i = 0; i < sources_len; i++) {
		if (str_equals(path, source_paths[i])) {
			return &sources[i];
		}
	}

	error_if(sources_len >= SOUND_MAX_SOURCES, "Max sound sources (%d) reached", SOUND_MAX_SOURCES);
	error_if(engine_is_running(), "Cant load sound source during gameplay");

	uint32_t file_size;
	uint8_t *data = platform_load_asset(path, &file_size);
	error_if(data == NULL, "Failed to load sound %s", path);

	qoa_desc desc;
	uint32_t read_pos = qoa_decode_header(data, file_size, &desc);
	error_if(read_pos == 0, "Failed to decode sound %s", path);
	error_if(desc.channels > 2, "QOA file %s has more than 2 channels", path);

	sound_source_t *source = &sources[sources_len];
	source->channels = desc.channels;
	source->len = desc.samples;
	source->samplerate = desc.samplerate;

	uint32_t total_samples = desc.samples * desc.channels;

	// Is this source short enough to completely uncompress it on load?
	if (total_samples <= SOUND_MAX_UNCOMPRESSED_SAMPLES) {
		source->type = SOUND_TYPE_PCM;
		source->pcm_samples = bump_alloc(total_samples * sizeof(int16_t));

		uint32_t sample_index = 0;
		uint32_t frame_len;
		uint32_t frame_size;

		do {
			int16_t *sample_ptr = source->pcm_samples + sample_index * desc.channels;
			frame_size = qoa_decode_frame(data + read_pos, file_size - read_pos, &desc, sample_ptr, &frame_len);
			error_if(frame_size == 0, "QOA decode error for file %s", path);

			read_pos += frame_size;
			sample_index += frame_len;
		} while (frame_size && sample_index < desc.samples);

		temp_free(data);
	}

	// Longer sources will be decompressed on demand; we just decode the first
	// frame here.
	else {
		uint32_t qoa_data_size = file_size - read_pos;

		// Transfer data to bump mem
		uint8_t *bump_data = bump_from_temp(data, read_pos, qoa_data_size);

		source->type = SOUND_TYPE_QOA;
		source->qoa = bump_alloc(sizeof(sound_source_qoa_t));
		source->qoa->desc = desc;
		source->qoa->data = bump_data;
		source->qoa->data_len = qoa_data_size;
		source->qoa->pcm_buffer_start = 0;
		source->qoa->pcm_buffer = bump_alloc(desc.channels * QOA_FRAME_LEN * sizeof(int16_t));

		uint32_t frame_len;
		uint32_t frame_size = qoa_decode_frame(
			source->qoa->data, file_size - read_pos, 
			&desc, source->qoa->pcm_buffer, &frame_len
		);
		error_if(frame_size == 0, "QOA decode error for file %s", path);
	}
	

	source_paths[sources_len] = bump_alloc(strlen(path)+1);
	strcpy(source_paths[sources_len], path);

	sources_len++;
	return source;
}

float sound_source_duration(sound_source_t *source) {
	return source->len / source->samplerate;
}



// sound node ------------------------------------------------------------------

sound_t sound(sound_source_t *source) {
	sound_t sound = {.id = 0, .index = 0};
	sound_node_t *node = NULL;

	// Get any node that is not currently playing
	for (int i = 0; i < SOUND_MAX_NODES; i++) {
		if (!sound_nodes[i].is_playing && !sound_nodes[i].is_halted && !sound_nodes[i].id){
			node = &sound_nodes[i];
			sound.index = i;
			break;
		}
	}

	// Fallback to any node that is not reserved; this will cut off
	// unreserved playing nodes
	if (!node) {
		for (int i = 0; i < SOUND_MAX_NODES; i++) {
			if (!sound_nodes[i].id) {
				node = &sound_nodes[i];
				sound.index = i;
				break;
			}
		}
	}

	// Still nothing?
	if (!node) {
		return sound;
	}

	sound_unique_id++;
	if (sound_unique_id == 0) {
		sound_unique_id = 1;
	}

	node->id = sound_unique_id;
	node->is_playing = false;
	node->is_halted = false;
	node->is_looping = false;
	node->source = source;
	node->volume = 1;
	node->pan = 0;
	node->sample_pos = 0;
	node->pitch = 1;

	sound.id = sound_unique_id;
	return sound;
}

void sound_play(sound_source_t *source) {
	sound_t s = sound(source);
	sound_unpause(s);
	sound_dispose(s);
}

void sound_play_ex(sound_source_t *source, float volume, float pan, float pitch) {
	sound_t s = sound(source);
	sound_set_volume(s, volume);
	sound_set_pan(s, pan);
	sound_set_pitch(s, pitch);
	sound_unpause(s);
	sound_dispose(s);
}


static inline sound_node_t *sound_get_node(sound_t sound) {
	if (sound.index < SOUND_MAX_NODES && sound_nodes[sound.index].id == sound.id) {
		return &sound_nodes[sound.index];
	}
	else {
		return NULL;
	}
}

void sound_unpause(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->is_playing = true;
	node->is_halted = false;
}

void sound_pause(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->is_playing = false;
	node->is_halted = false;
}

void sound_stop(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->sample_pos = 0;
	node->is_playing = false;
	node->is_halted = false;
}

void sound_dispose(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->is_looping = false;
	node->id = 0;
}

bool sound_loop(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return false;
	}

	return node->is_looping;
}

void sound_set_loop(sound_t sound, bool loop) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->is_looping = loop;
}

float sound_duration(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return sound_source_duration(node->source);
}

float sound_time(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->sample_pos / node->source->samplerate;
}

void sound_set_time(sound_t sound, float time) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->sample_pos = clamp(time / node->source->samplerate, 0, node->source->len);
}

float sound_volume(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->volume;
}

void sound_set_volume(sound_t sound, float volume) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->volume = clamp(volume, 0, 16);
}

float sound_pan(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->pan;
}

void sound_set_pan(sound_t sound, float pan) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->pan = clamp(pan, -1, 1);
}

float sound_pitch(sound_t sound) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return 0;
	}

	return node->pitch;
}

void sound_set_pitch(sound_t sound, float pitch) {
	sound_node_t *node = sound_get_node(sound);
	if (!node) {
		return;
	}

	node->pitch = pitch;
}
