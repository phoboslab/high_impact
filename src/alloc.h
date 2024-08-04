#ifndef HI_ALLOC_H
#define HI_ALLOC_H

// We statically reserve a single "hunk" of memory at program start. Memory
// (for our own allocators) can not ever outgrow this hunk. Theres two ways to
// allocate bytes from this hunk:

//   1. A bump allocator that just grows linearly and may be reset to a previous 
// level. This returns bytes from the front of the hunk and is meant for all
// data the game needs while it's running.

// high_impact mostly manages this bump level for you. First everything that is 
// bump-allocated _before_ engine_set_scene() is called will only be freed when.
// the program ends.
// Then, when a scene is loaded the bump position is recorded. When the current
// scene ends (i.e. engine_set_scene() is called again), the bump allocator is
// reset to that position. Conceptually the scene is wrapped in an alloc_pool().
// Thirdly, each frame is wrapped in an alloc_pool().

// This all means that you can't use any memory that you allocated in one scene
// in another scene and also that you can't use any memory that you allocated in
// one frame in the next frame.

//   2. A temp allocator. This allocates bytes from the end of the hunk. Temp
// allocated bytes must be explicitly temp_freed() again. As opposed to the bump
// allocater, the temp allocator can be freed() out of order.

// The temp allocator is meant for very short lived objects, to assist data
// loading. E.g. pixel data from an image file might be temp allocated, handed
// over to the render (which may pass it on to the GPU or permanently put it in
// the bump memory) and then immediately free() it again.

// Temp allocations are not allowed to persist. At the end of each frame, the
// engine checks if the temp allocator is empty - and if not: kills the program.

// There's no way to handle an allocation failure. We just kill the program
// with an error. This is fine if you know all your game data (i.e. levels) in
// advance. Games that allow loading user defined levels may need a separate 
// allocation strategy...

#include "types.h"

// The total size of the hunk
#if !defined(ALLOC_SIZE)
	#define ALLOC_SIZE (32 * 1024 * 1024)
#endif

// The max number of temp objects to be allocated at a time
#if !defined(ALLOC_TEMP_OBJECTS_MAX)
	#define ALLOC_TEMP_OBJECTS_MAX 8
#endif


typedef struct { uint32_t index; } bump_mark_t;

// Return the current position of the bump allocator
bump_mark_t bump_mark(void);

// Allocate `size` bytes in bump memory
void *bump_alloc(uint32_t size);

// Reset the bump allocator to the given position
void bump_reset(bump_mark_t mark);

// Move bytes from temp to bump memory. This is essentially a shorthand for
// `bump_alloc(); memcpy(); temp_free();` without the requirement to fit both 
// (temp and bump) into the hunk at the same time.
void *bump_from_temp(void *temp, uint32_t offset, uint32_t size);

// `alloc_pool() { ... }` is a shorthand for `bump_mark()` and `bump_reset()`.
// You can use this to wrap allocations that you only need momentarily. E.g.:
// alloc_pool() {
//     result = memory_intensive_computation_that_bump_allocates();
//     do_something(result);
// }
// Be careful to NOT `return` from within an `alloc_pool() {...}` - bump_reset() 
// will not be called if you do. 
// FIXME: maybe this macro is too error prone and should be removed?
#define alloc_pool() \
	for( \
		bump_mark_t _bump_mark_##__LINE__ = bump_mark(); \
		_bump_mark_##__LINE__.index != 0xFFFFFFFF; \
		bump_reset(_bump_mark_##__LINE__), _bump_mark_##__LINE__.index = 0xFFFFFFFF \
	)


// Allocate `size` bytes in temp memory
void *temp_alloc(uint32_t size);

// Free the temp allocation
void temp_free(void *p);

// Check if temp is empty, or die()
void temp_alloc_check(void);

#endif
