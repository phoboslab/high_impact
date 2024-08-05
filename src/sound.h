#ifndef HI_SOUND_H
#define HI_SOUND_H

// Sounds are split in two parts: the sound source (sound_source_t) and a "node"
// (sound_t) representing a currently playing sound, using one of the sources.

#include "types.h"

// The maximum number of samples for which a sound source is decompressed
// completely at load time. Everything above this limit will be loaded into 
// memory in compressed form and only decompressed on demand.
#if !defined(SOUND_MAX_UNCOMPRESSED_SAMPLES)
	#define SOUND_MAX_UNCOMPRESSED_SAMPLES (64 * 1024)
#endif

// The maximum number of sources to be loaded at a time. This only affects
// memory usage, but not performance.
#if !defined(SOUND_MAX_SOURCES)
	#define SOUND_MAX_SOURCES 128
#endif

// The maximum number of active nodes that can be mixed at time
#if !defined(SOUND_MAX_NODES)
	#define SOUND_MAX_NODES 32
#endif


typedef struct sound_source_t sound_source_t;
typedef struct { uint16_t id; uint16_t index; } sound_t;
typedef struct { uint32_t index; } sound_mark_t;

// Called by the engine to manage sound memory
sound_mark_t sound_mark(void);
void sound_reset(sound_mark_t mark);

// Put all playing nodes in a halt state; usefull for e.g. a pause screen
void sound_halt(void);

// Resume playing all halted sounds
void sound_resume(void);

// Return the global volume for all sounds
float sound_global_volume(void);

// Set the global volume for all nodes
void sound_set_global_volume(float volume);

// Periodically called by the platform to mix playing nodes into output buffer
void sound_mix_stereo(float *dest_samples, uint32_t dest_len);

// Load a sound sorce from a QOA file. Calling this function multiple times with
// the same path will return the same, cached sound source,
sound_source_t *sound_source(char *path);

// Return the duration of a sound source
float sound_source_duration(sound_source_t *source);

// Obtain a free node for the given source. This will "reserve" the source. It 
// can not be re-used until it is disposed via sound_dispose(). The node will be
// in a paused state and must be explicitly unpaused. Returns an invalid node
// with id = 0 when no free node is available.
sound_t sound(sound_source_t *source);

// Play a sound source. The node used to play it will be automatically disposed
// once it has played through.
void sound_play(sound_source_t *source);

// Play a sound source with the given volume and pitch. The node used to play it
// will be automatically disposed once it has played through.
void sound_play_ex(sound_source_t *source, float volume, float pan, float pitch);

// Unpauses a paused node
void sound_unpause(sound_t sound);

// Pauses a node
void sound_pause(sound_t sound);

// Pauses a node and rewind it to the start
void sound_stop(sound_t sound);

// Dispose this node. The node is invalid afterwards, but will still play to the
// end if it's not paused.
void sound_dispose(sound_t sound);

// Return whether this node loops
bool sound_loop(sound_t sound);

// Set whether to loop this node
void sound_set_loop(sound_t sound, bool loop);

// Return the duration in seconds of the underlying sound source. This does not
// take the node's current pitch into account
float sound_duration(sound_t sound);

// Return the current position of this node in seconds. This does not take the 
// node's current pitch into account
float sound_time(sound_t sound);

// Set the current position of this node in seconds. This does not take the 
// node's current pitch into account
void sound_set_time(sound_t sound, float time);

// Return the current volume of this node
float sound_volume(sound_t sound);

// Set the current volume of this node
void sound_set_volume(sound_t sound, float volume);

// Return the current pan of the node (-1 = left, 0 = center, 1 = right)
float sound_pan(sound_t sound);

// Return the current pan of a the node
void sound_set_pan(sound_t sound, float pan);

// Return the current pitch (playback speed) of this node. Default 1.
float sound_pitch(sound_t sound);

// Set the current pitch (playback speed) of this node
void sound_set_pitch(sound_t sound, float pitch);

// Called by the platform
void sound_init(int samplerate);
void sound_cleanup(void);

#endif
