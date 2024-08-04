#include "platform.h"

#if defined(PLATFORM_SDL)
	#include "platform_sdl.c"
#elif defined(PLATFORM_SOKOL)
	#include "platform_sokol.c"
#else
	#error "No platform specified. #define PLATFORM_SDL or PLATFORM_SOKOL"
#endif

json_t *platform_load_asset_json(const char *name) {
	uint32_t len;
	uint8_t *data = platform_load_asset(name, &len);
	if (data == NULL) {
		return NULL;
	}
	json_t *v = json_parse(data, len);
	temp_free(data);
	return v;
}
