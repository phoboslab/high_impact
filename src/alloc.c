#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "utils.h"

static uint8_t hunk[ALLOC_SIZE];
static uint32_t bump_len = 0;
static uint32_t temp_len = 0;

static uint32_t temp_objects[ALLOC_TEMP_OBJECTS_MAX] = {};
static uint32_t temp_objects_len;

bump_mark_t bump_mark(void) {
	return (bump_mark_t){.index = bump_len};
}

void *bump_alloc(uint32_t size) {
	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in hunk mem", size);
	void *p = &hunk[bump_len];
	bump_len += size;
	memset(p, 0, size);
	return p;
}

void bump_reset(bump_mark_t mark) {
	error_if(mark.index > ALLOC_SIZE, "Invalid mem reset");
	bump_len = mark.index;
}

void *bump_from_temp(void *temp, uint32_t offset, uint32_t size) {
	temp_free(temp);
	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in hunk mem", size);
	void *p = &hunk[bump_len];
	bump_len += size;
	memmove(p, (uint8_t *)temp + offset, size);
	return p;
}

void *temp_alloc(uint32_t size) {
	size = ((size + 7) >> 3) << 3; // allign to 8 bytes

	error_if(bump_len + temp_len + size >= ALLOC_SIZE, "Failed to allocate %d bytes in temp mem", size);
	error_if(temp_objects_len >= ALLOC_TEMP_OBJECTS_MAX, "ALLOC_TEMP_OBJECTS_MAX reached");

	temp_len += size;
	void *p = &hunk[ALLOC_SIZE - temp_len];
	temp_objects[temp_objects_len++] = temp_len;
	return p;
}

void temp_free(void *p) {
	uint32_t offset = (uint8_t *)&hunk[ALLOC_SIZE] - (uint8_t *)p;
	error_if(offset > ALLOC_SIZE, "Object 0x%p not in temp hunk", p);

	bool found = false;
	uint32_t remaining_max = 0;
	for (uint32_t i = 0; i < temp_objects_len; i++) {
		if (temp_objects[i] == offset) {
			temp_objects[i--] = temp_objects[--temp_objects_len];
			found = true;
		}
		else if (temp_objects[i] > remaining_max) {
			remaining_max = temp_objects[i];
		}
	}
	error_if(!found, "Object 0x%p not in temp hunk", p);
	temp_len = remaining_max;
}

void temp_alloc_check(void) {
	error_if(temp_len != 0, "Temp memory not free: %d object(s)", temp_objects_len);
}
