#ifndef HI_UTILS_H
#define HI_UTILS_H

// Various math and utility functions
// FIXME: some of these are unused and should probably be removed?

#include <string.h>
#include "types.h"
#include "../libs/pl_json.h"

#ifdef WIN32
	#undef min
	#undef max
#endif

#if !defined(offsetof)
	#define offsetof(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
#endif
#define member_size(type, member) sizeof(((type *)0)->member)

// The larger value of a and b
#define max(a, b) ({ \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; \
	})

// The smaller value of a and b
#define min(a, b) ({ \
		__typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; \
	})

// Swap a and b
#define swap(a, b) ({ \
		__typeof__(a) tmp = a; a = b; b = tmp; \
	})

// Clamp v between min and max
#define clamp(v, min, max) ({ \
		__typeof__(v) _v = v, _min = min, _max = max; \
		_v > _max ? _max : _v < _min ? _min : _v; \
	})

// Scales v from the input range to the output range. This is useful for all 
// kinds of transitions. E.g. to move an image in from the right side of the
// screen to the center over 2 second, starting at the 3 second:
// x = scale(time, 3, 5, screen_size.x, screen_size.x/2)
#define scale(v, in_min, in_max, out_min, out_max) ({ \
		__typeof__(v) _in_min = in_min, _out_min = out_min; \
		_out_min + ((out_max) - _out_min) * (((v) - _in_min) / ((in_max) - _in_min)); \
	})

// Linearly interpolate from a to b over normalized 0..1 t
#define lerp(a, b, t) ({ \
		__typeof__(a) _a = a; \
		_a + ((b) - _a) * (t); \
	})

#define to_radians(A) ((A) * (M_PI/180.0))
#define to_degrees(R) ((R) * (180.0/M_PI))

// The length in elements of a statically allocated array
#define len(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))

// Clear a statically allocated array or structure to 0
#define clear(A) memset(A, 0, sizeof(A))

// Careful, this is an insertion sort. It's fine for mostly sorted data (i.e. 
// if you sort the same array for every frame) but will blow up to O(n^2) for
// unsorted data. FIXME!?
#define sort(LIST, LEN, COMPARE_FUNC) \
	for (uint32_t sort_i = 1, sort_j; sort_i < (LEN); sort_i++) { \
		sort_j = sort_i; \
		__typeof__((LIST)[0]) sort_temp = (LIST)[sort_j]; \
		while (sort_j > 0 && COMPARE_FUNC((LIST)[sort_j-1], sort_temp)) { \
			(LIST)[sort_j] = (LIST)[sort_j-1]; \
			sort_j--; \
		} \
		(LIST)[sort_j] = sort_temp; \
	}

// A fair Fisher-Yates shuffle
#define shuffle(LIST, LEN) \
	for (int i = (LEN) - 1; i > 0; i--) { \
		int j = rand_int(0, i); \
		swap((LIST)[i], (LIST)[j]); \
	}

static inline float round_to_precision(float v, float p) {
	return roundf(v * p) / p;
}


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define die(...) \
	printf("Abort at " TOSTRING(__FILE__) " line " TOSTRING(__LINE__) ": " __VA_ARGS__); \
	printf("\n"); \
	exit(1)

#define error_if(TEST, ...) \
	if (TEST) { \
		die(__VA_ARGS__); \
	}

// Whether string haystack starts with needle
bool str_starts_with(const char *haystack, const char *needle);

// Whether string a equals b
bool str_equals(const char *a, const char *b);

// Whether string haystack contains needle
bool str_contains(const char *haystack, const char *needle);

// A wrapper around sprintf that allocates in bump memory
char *str_format(const char *format, ...);

// Seed the random number generator to a particular state
void rand_seed(uint64_t s);

// A random uint64_t
uint64_t rand_uint64(void);

// A random float between min and max
float rand_float(float min, float max);

// A random int between min and max (inclusive)
int32_t rand_int(int32_t min, int32_t max); 


// These are low-level file loading functions that do not take the asset or user
// directory into account. You should probably use platform_load_asset() and
// friends instead.

// Whether the file at path exists
bool file_exists(const char *path);

// Load the file at path completely into temp memory. Must be explicitly freed
// with temp_free(). Returns NULL on failure.
uint8_t *file_load(const char *path, uint32_t *bytes_read);

// Writes bytes with len into the file at path. Returns the number of bytes
// written, or 0 on failure.
uint32_t file_store(const char *path, void *bytes, int32_t len);

// Parse the string data into temp allocated json. Must be explicitly freed
// with temp_free(). Returns NULL on failure.
json_t *json_parse(uint8_t *data, uint32_t len);

#endif
