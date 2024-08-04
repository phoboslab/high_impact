#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "utils.h"
#include "alloc.h"

#define PL_JSON_IMPLEMENTATION
#include "../libs/pl_json.h"

bool file_exists(const char *path) {
	struct stat s;
	return (stat(path, &s) == 0);
}

int file_size(FILE *f) {
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

uint8_t *file_load(const char *path, uint32_t *bytes_read) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}

	int size = file_size(f);
	if (size <= 0) {
		fclose(f);
		return NULL;
	}

	uint8_t *bytes = temp_alloc(size);
	*bytes_read = fread(bytes, 1, size, f);
	fclose(f);

	if (*bytes_read != size) {
		*bytes_read = 0;
		temp_free(bytes);
		return NULL;
	}
	
	return bytes;
}

uint32_t file_store(const char *path, void *bytes, int32_t len) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		return 0;
	}

	int32_t bytes_written = fwrite(bytes, 1, len, f);
	fclose(f);

	if (bytes_written != len) {
		return 0;
	}

	return len;
}

bool str_equals(const char *a, const char *b) {
	return (strcmp(a, b) == 0);
}

bool str_starts_with(const char *haystack, const char *needle) {
	return (strncmp(haystack, needle, strlen(needle)) == 0);
}

bool str_contains(const char *haystack, const char *needle) {
	return strstr(haystack, needle) != NULL;
}

char *str_format(const char *format, ...) {
	va_list args;
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args) + 1;
	va_end(args);
	if (len <= 1) {
		char *buf = bump_alloc(1);
		buf[0] = '\0';
		return buf;
	}

	va_start(args, format);
	char *buf = bump_alloc(len);
	vsnprintf(buf, len, format, args);
	return buf;
}

static uint64_t rand_uint64_state[2] = {0xdf900294d8f554a5, 0x170865df4b3201fc};

void rand_seed(uint64_t s) {
	uint64_t z = (s += 0x9e3779b97f4a7c15);
	rand_uint64_state[0] = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	rand_uint64_state[1] = (z ^ (z >> 27)) * 0x94d049bb133111eb;
}

uint64_t rand_uint64(void) {
	// https://prng.di.unimi.it/
    uint64_t s0 = rand_uint64_state[0];
    uint64_t s1 = rand_uint64_state[1];
    uint64_t result = s0 + s1;
    s1 ^= s0;
    rand_uint64_state[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
    rand_uint64_state[1] = (s1 << 36) | (s1 >> 28);
    return result;
}

float rand_float(float min, float max) {
	return min + ((float)rand_uint64() / (float)UINT64_MAX) * (max - min);
}

int32_t rand_int(int32_t min, int32_t max) {
	return min + rand_uint64() % (max - min + 1);
}

json_t *json_parse(uint8_t *data, uint32_t len) {
	uint32_t size_req = 0;
	uint32_t tokens_capacity = 1 + len / 2;

	json_token_t *tokens = temp_alloc(tokens_capacity * sizeof(json_token_t));
	int32_t tokens_len = json_tokenize((char *)data, len, tokens, tokens_capacity, &size_req);
	if (tokens_len <= 0) {
		return NULL;
	}

	json_t *v = temp_alloc(size_req);
	json_parse_tokens((char *)data, tokens, tokens_len, v);
	temp_free(tokens);
	return v;
}
