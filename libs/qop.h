/*

Copyright (c) 2024, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


QOP - The “Quite OK Package Format” for bare bones file packages


// Define `QOP_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define QOP_IMPLEMENTATION
#include "qop.h"


-- File format description (pseudo code)

struct {
	// Path string and data of all files in this archive
	struct {
		uint8_t path[path_len];
		uint8_t bytes[size];
	} file_data[];

	// The index, with a list of files
	struct {
		uint64_t hash;
		uint32_t offset;
		uint32_t size;
		uint16_t path_len;
		uint16_t flags;
	} qop_file[];

	// The number of files in the index
	uint32_t index_len;

	// The size of the whole archive, including the header
	uint32_t archive_size; 

	// Magic bytes "qopf"
	uint32_t magic;
} qop;


*/


/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef QOP_H
#define QOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

#define QOP_FLAG_NONE               0
#define QOP_FLAG_COMPRESSED_ZSTD    (1 << 0)
#define QOP_FLAG_COMPRESSED_DEFLATE (1 << 1)
#define QOP_FLAG_ENCRYPTED          (1 << 8)

typedef struct {
	unsigned long long hash;
	unsigned int offset;
	unsigned int size;
	unsigned short path_len;
	unsigned short flags;
} qop_file;

typedef struct {
	FILE *fh;
	qop_file *hashmap;
	unsigned int files_offset;
	unsigned int index_offset;
	unsigned int index_len;
	unsigned int hashmap_len;
	unsigned int hashmap_size;
} qop_desc;

// Open an archive at path. The supplied qop_desc will be filled with the
// information from the file header. Returns the size of the archvie or 0 on
// failure
int qop_open(const char *path, qop_desc *qop);

// Read the index from an opened archive. The supplied buffer will be filled
// with the index data and must be at least qop->hashmap_size bytes long.
// No ownership is taken of the buffer; if you allocated it with malloc() you
// need to free() it yourself after qop_close();
// Returns the number of files in the archive or 0 on error.
int qop_read_index(qop_desc *qop, void *buffer);

// Close the archive
void qop_close(qop_desc *qop);

// Find a file with the supplied path. Returns NULL if the file is not found
qop_file *qop_find(qop_desc *qop, const char *path);

// Copy the path of the file into dest. The dest buffer must be at least 
// file->path_len bytes long. The path is null terminated.
// Returns the path length (including the null terminater) or 0 on error.
int qop_read_path(qop_desc *qop, qop_file *file, char *dest);

// Read the whole file into dest. The dest buffer must be at least file->size
// bytes long.
// Returns the number of bytes read
int qop_read(qop_desc *qop, qop_file *file, unsigned char *dest);

// Read part of a file into dest. The dest buffer must be at least len bytes
// long.
// Returns the number of bytes read.
int qop_read_ex(qop_desc *qop, qop_file *file, unsigned char *dest, unsigned int start, unsigned int len);


#ifdef __cplusplus
}
#endif
#endif /* QOP_H */



/* -----------------------------------------------------------------------------
Implementation */

#ifdef QOP_IMPLEMENTATION

typedef unsigned long long qop_uint64_t;

#define QOP_MAGIC \
	(((unsigned int)'q') <<  0 | ((unsigned int)'o') <<  8 | \
	 ((unsigned int)'p') << 16 | ((unsigned int)'f') << 24)
#define QOP_HEADER_SIZE 12
#define QOP_INDEX_SIZE 20

// MurmurOAAT64
static inline qop_uint64_t qop_hash(const char *key) {
	qop_uint64_t h = 525201411107845655ull;
	for (;*key;++key) {
		h ^= (unsigned char)*key;
		h *= 0x5bd1e9955bd1e995ull;
		h ^= h >> 47;
	}
  	return h;
}

static unsigned short qop_read_16(FILE *fh) {
	unsigned char b[sizeof(unsigned short)] = {0};
	if (fread(b, sizeof(unsigned short), 1, fh) != 1) {
		return 0;
	}
	return (b[1] << 8) | b[0];
}

static unsigned int qop_read_32(FILE *fh) {
	unsigned char b[sizeof(unsigned int)] = {0};
	if (fread(b, sizeof(unsigned int), 1, fh) != 1) {
		return 0;
	}
	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
}

static qop_uint64_t qop_read_64(FILE *fh) {
	unsigned char b[sizeof(qop_uint64_t)] = {0};
	if (fread(b, sizeof(qop_uint64_t), 1, fh) != 1) {
		return 0;
	}
	return 
		((qop_uint64_t)b[7] << 56) | ((qop_uint64_t)b[6] << 48) | 
		((qop_uint64_t)b[5] << 40) | ((qop_uint64_t)b[4] << 32) |
		((qop_uint64_t)b[3] << 24) | ((qop_uint64_t)b[2] << 16) | 
		((qop_uint64_t)b[1] <<  8) | ((qop_uint64_t)b[0]);
}

int qop_open(const char *path, qop_desc *qop) {
	FILE *fh = fopen(path, "rb");
	if (!fh) {
		return 0;
	}

	fseek(fh, 0, SEEK_END);
	int size = ftell(fh);
	if (size <= QOP_HEADER_SIZE || fseek(fh, size - QOP_HEADER_SIZE, SEEK_SET) != 0) {
		fclose(fh);
		return 0;
	}

	qop->fh = fh;
	qop->hashmap = NULL;
	unsigned int index_len = qop_read_32(fh);
	unsigned int archive_size = qop_read_32(fh);
	unsigned int magic = qop_read_32(fh);

	// Check magic, make sure index_len is possible with the file size
	if (
		magic != QOP_MAGIC ||
		index_len * QOP_INDEX_SIZE > (unsigned int)(size - QOP_HEADER_SIZE)
	) {
		fclose(fh);
		return 0;
	}

	// Find a good size for the hashmap: power of 2, at least 1.5x num entries
	unsigned int hashmap_len = 1;
	unsigned int min_hashmap_len = index_len * 1.5;
	while (hashmap_len < min_hashmap_len) {
		hashmap_len <<= 1;
	}

	qop->files_offset  = size - archive_size;
	qop->index_len = index_len;
	qop->index_offset = size - qop->index_len * QOP_INDEX_SIZE - QOP_HEADER_SIZE;
	qop->hashmap_len = hashmap_len;
	qop->hashmap_size = qop->hashmap_len * sizeof(qop_file);
	return size;	
}

int qop_read_index(qop_desc *qop, void *buffer) {
	qop->hashmap = buffer;
	int mask = qop->hashmap_len - 1;

	memset(qop->hashmap, 0, qop->hashmap_size);
	fseek(qop->fh, qop->index_offset, SEEK_SET);

	for (unsigned int i = 0; i < qop->index_len; i++) {
		qop_uint64_t hash = qop_read_64(qop->fh);

		int idx = hash & mask;
		while (qop->hashmap[idx].size > 0) {
			idx = (idx + 1) & mask;
		}
		qop->hashmap[idx].hash     = hash;
		qop->hashmap[idx].offset   = qop_read_32(qop->fh);
		qop->hashmap[idx].size     = qop_read_32(qop->fh);
		qop->hashmap[idx].path_len = qop_read_16(qop->fh);
		qop->hashmap[idx].flags    = qop_read_16(qop->fh);
	}
	return qop->index_len;
}

void qop_close(qop_desc *qop) {
	fclose(qop->fh);
}

qop_file *qop_find(qop_desc *qop, const char *path) {
	if (qop->hashmap == NULL) {
		return NULL;
	}

	int mask = qop->hashmap_len - 1;

	qop_uint64_t hash = qop_hash(path);
	int idx = hash & mask;
	while (qop->hashmap[idx].size > 0) {
		if (qop->hashmap[idx].hash == hash) {
			return &qop->hashmap[idx];
		}
		idx = (idx + 1) & mask;
	}
	return NULL;
}

int qop_read_path(qop_desc *qop, qop_file *file, char *dest) {
	fseek(qop->fh, qop->files_offset + file->offset, SEEK_SET);
	return fread(dest, 1, file->path_len, qop->fh);
}

int qop_read(qop_desc *qop, qop_file *file, unsigned char *dest) {
	fseek(qop->fh, qop->files_offset + file->offset + file->path_len, SEEK_SET);
	return fread(dest, 1, file->size, qop->fh);
}

int qop_read_ex(qop_desc *qop, qop_file *file, unsigned char *dest, unsigned int start, unsigned int len) {
	fseek(qop->fh, qop->files_offset + file->offset + file->path_len + start, SEEK_SET);
	return fread(dest, 1, len, qop->fh);
}


#endif /* QOP_IMPLEMENTATION */
