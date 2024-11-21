#include "platform.h"

#if defined(PLATFORM_SDL)
	#include "platform_sdl.c"
#elif defined(PLATFORM_SOKOL)
	#include "platform_sokol.c"
#else
	#error "No platform specified. #define PLATFORM_SDL or PLATFORM_SOKOL"
#endif

// Dependencies for platform_get_base_path()
#if defined(_WIN32)
	#include <windows.h>
#elif defined(__APPLE__)
	#include <mach-o/dyld.h>
#elif defined(__linux__)
	#include <unistd.h>
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

char *platform_executable_path(void) {
	uint32_t buffer_len = 2048;
	char buffer[buffer_len];

	#if defined(_WIN32)
		if (GetModuleFileName(NULL, buffer, (DWORD)buffer_len) == 0) {
			return NULL;
		}
	#elif defined(__APPLE__)
		if (_NSGetExecutablePath(buffer, &buffer_len) != 0) {
			return NULL;
		}
	#elif defined(__linux__)
		ssize_t len = readlink("/proc/self/exe", buffer, buffer_len - 1);
		if (len < 0) {
			return NULL;
		}
		buffer[len] = '\0';
	#else
		return NULL;
	#endif

	return str_format("%s", buffer);
}

char *platform_dirname(char *path) {
	#if defined(_WIN32)
		// On Windows, find the last back- or forward slash
		char *last_slash = max(strrchr(path, '/'), strrchr(path, '\\'));
	#else
		char *last_slash = strrchr(path, '/');
	#endif
	if (last_slash == NULL) {
		return str_format("");
	}
	return str_format("%.*s", last_slash - path + 1, path);
}
