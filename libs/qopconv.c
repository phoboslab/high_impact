/*

Copyright (c) 2024, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


Command line tool to create and unpack qop archives

*/

#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define QOP_IMPLEMENTATION
#include "qop.h"

#define MAX_PATH_LEN 1024
#define BUFFER_SIZE 4096

#define UNUSED(x) (void)(x)
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


// -----------------------------------------------------------------------------
// Platform specific file/dir handling

typedef struct {
	char *name;
	unsigned char is_dir;
	unsigned char is_file;
} pi_dirent;

#if defined(_WIN32)
	#include <windows.h>

	typedef struct {
		WIN32_FIND_DATA data;
		pi_dirent current;
		HANDLE dir;
		unsigned char is_first;
	} pi_dir;
		
	pi_dir *pi_dir_open(const char *path) {
		char find_str[MAX_PATH_LEN];
		snprintf(find_str, MAX_PATH_LEN, "%s/*", path);

		pi_dir *d = malloc(sizeof(pi_dir));
		d->is_first = 1;
		d->dir = FindFirstFile(find_str, &d->data);
		if (d->dir == INVALID_HANDLE_VALUE) {
			free(d);
			return NULL;
		}
		return d;
	}

	pi_dirent *pi_dir_next(pi_dir *d) {
		if (!d->is_first) {
			if (FindNextFile(d->dir, &d->data) == 0) {
				return NULL;
			}
		}
		d->is_first = 0;
		d->current.name = d->data.cFileName;
		d->current.is_dir = d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		d->current.is_file = !d->current.is_dir;
		return &d->current;
	}

	void pi_dir_close(pi_dir *d) {
		FindClose(d->dir);
		free(d);
	}

	int pi_mkdir(char *path, int mode) {
		UNUSED(mode);
		return CreateDirectory(path, NULL) ? 0 : -1;
	}
#else
	#include <dirent.h>
	
	typedef struct {
		DIR *dir;
		struct dirent *data;
		pi_dirent current;
	} pi_dir;
		
	pi_dir *pi_dir_open(const char *path) {
		DIR *dir = opendir(path);
		if (!dir) {
			return NULL;
		}
		pi_dir *d = malloc(sizeof(pi_dir));
		d->dir = dir;
		return d;
	}

	pi_dirent *pi_dir_next(pi_dir *d) {
		d->data = readdir(d->dir);
		if (!d->data) {
			return NULL;
		}
		d->current.name = d->data->d_name;
		d->current.is_dir = d->data->d_type & DT_DIR;
		d->current.is_file = d->data->d_type == DT_REG;
		return &d->current;
	}

	void pi_dir_close(pi_dir *d) {
		closedir(d->dir);
		free(d);
	}

	int pi_mkdir(char *path, int mode) {
		return mkdir(path, mode);
	}
#endif


// -----------------------------------------------------------------------------
// Unpack

int create_path(const char *path, const mode_t mode) {
	char tmp[MAX_PATH_LEN];
	char *p = NULL;
	struct stat sb;
	size_t len;

	// copy path
	len = strnlen(path, MAX_PATH_LEN);
	if (len == 0 || len == MAX_PATH_LEN) {
		return -1;
	}
	memcpy(tmp, path, len);
	tmp[len] = '\0';

	// remove file part
	char *last_slash = strrchr(tmp, '/');
	if (last_slash == NULL) {
		return 0;
	}
	*last_slash = '\0';

	// check if path exists and is a directory
	if (stat(tmp, &sb) == 0) {
		if (S_ISDIR(sb.st_mode)) {
			return 0;
		}
	}

	// recursive mkdir
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (stat(tmp, &sb) != 0) {
				if (pi_mkdir(tmp, mode) < 0) {
					return -1;
				}
			}
			else if (!S_ISDIR(sb.st_mode)) {
				return -1;
			}
			*p = '/';
		}
	}
	if (stat(tmp, &sb) != 0) {
		if (pi_mkdir(tmp, mode) < 0) {
			return -1;
		}
	}
	else if (!S_ISDIR(sb.st_mode)) {
		return -1;
	}
	return 0;
}

unsigned int copy_out(FILE *src, unsigned int offset, unsigned int size, const char *dest_path) {
	FILE *dest = fopen(dest_path, "wb");
	error_if(!dest, "Could not open file %s for writing", dest_path);

	char buffer[BUFFER_SIZE];
	size_t bytes_read, bytes_written;
	unsigned int bytes_total = 0;
	unsigned int read_size = size < BUFFER_SIZE ? size : BUFFER_SIZE;

	fseek(src, offset, SEEK_SET);
	while (read_size > 0 && (bytes_read = fread(buffer, 1, read_size, src)) > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dest);
		error_if(bytes_written != bytes_read, "Write error");
		bytes_total += bytes_written;
		if (bytes_total >= size) {
			break;
		}
		if (size - bytes_total < read_size) {
			read_size = size - bytes_total;
		}
	}

	error_if(ferror(src), "read error for file %s", dest_path);
	fclose(dest);
	return bytes_total;
}

void unpack(const char *archive_path, int list_only) {
	qop_desc qop;
	int archive_size = qop_open(archive_path, &qop);
	error_if(archive_size == 0, "Could not open archive %s", archive_path);

	// Read the archive index
	int index_len = qop_read_index(&qop, malloc(qop.hashmap_size));
	error_if(index_len == 0, "Could not read index from archive %s", archive_path);
	
	// Extract all files
	for (unsigned int i = 0; i < qop.hashmap_len; i++) {
		qop_file *file = &qop.hashmap[i];
		if (file->size == 0) {
			continue;
		}
		error_if(file->path_len >= MAX_PATH_LEN, "Path for file %016llx exceeds %d", file->hash, MAX_PATH_LEN);
		char path[MAX_PATH_LEN];
		qop_read_path(&qop, file, path);

		// Integrity check
		// error_if(!qop_find(&qop, path), "could not find %s", path);

		printf("%6d %016llx %10d %s\n", i, file->hash, file->size, path);

		if (!list_only) {
			error_if(create_path(path, 0755) != 0, "Could not create path %s", path);
			copy_out(qop.fh, qop.files_offset + file->offset + file->path_len, file->size, path);
		}
	}

	free(qop.hashmap);
	qop_close(&qop);
}


// -----------------------------------------------------------------------------
// Pack

typedef struct {
	qop_file *files;
	int len;
	int capacity;
	int size;
} pack_state;

void write_16(unsigned int v, FILE *fh) {
	unsigned char b[sizeof(unsigned short)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	int written = fwrite(b, sizeof(unsigned short), 1, fh);
	error_if(!written, "Write error");
}

void write_32(unsigned int v, FILE *fh) {
	unsigned char b[sizeof(unsigned int)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	b[2] = 0xff & (v >> 16);
	b[3] = 0xff & (v >> 24);
	int written = fwrite(b, sizeof(unsigned int), 1, fh);
	error_if(!written, "Write error");
}

void write_64(qop_uint64_t v, FILE *fh) {
	unsigned char b[sizeof(qop_uint64_t)];
	b[0] = 0xff & (v      );
	b[1] = 0xff & (v >>  8);
	b[2] = 0xff & (v >> 16);
	b[3] = 0xff & (v >> 24);
	b[4] = 0xff & (v >> 32);
	b[5] = 0xff & (v >> 40);
	b[6] = 0xff & (v >> 48);
	b[7] = 0xff & (v >> 56);
	int written = fwrite(b, sizeof(qop_uint64_t), 1, fh);
	error_if(!written, "Write error");
}

unsigned int copy_into(const char *src_path, FILE *dest) {
	FILE *src = fopen(src_path, "rb");
	error_if(!src, "Could not open file %s for reading", src_path);

	char buffer[BUFFER_SIZE];
	size_t bytes_read, bytes_written;
	unsigned int bytes_total = 0;

	while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
		bytes_written = fwrite(buffer, 1, bytes_read, dest);
		error_if(bytes_written != bytes_read, "Write error");
		bytes_total += bytes_written;
	}

	error_if(ferror(src), "read error for file %s", src_path);
	fclose(src);
	return bytes_total;
}

void add_file(const char *path, FILE *dest, pack_state *state) {
	if (state->len >= state->capacity) {
		state->capacity *= 2;
		state->files = realloc(state->files, state->capacity * sizeof(qop_file));
	}

	qop_uint64_t hash = qop_hash(path);
	

	// Write the path into the archive
	int path_len = strlen(path) + 1;
	int path_written = fwrite(path, sizeof(char), path_len, dest);
	error_if(path_written != path_len, "Write error");

	// Copy the file into the archive
	unsigned int size = copy_into(path, dest);

	printf("%6d %016llx %10d %s\n", state->len, hash, size, path);

	// Collect file info for the index
	state->files[state->len] = (qop_file){
		.hash = hash,
		.offset = state->size,
		.size = size,
		.path_len = path_len,
		.flags = QOP_FLAG_NONE
	};
	state->size += size + path_len;
	state->len++;
}

void add_dir(const char *path, FILE *dest, pack_state *state) {
	pi_dir *dir = pi_dir_open(path);
	error_if(!dir, "Could not open directory %s for reading", path);

	pi_dirent *entry;
	while ((entry = pi_dir_next(dir))) {
		if (
			entry->is_dir &&
			strcmp(entry->name, ".") != 0 &&
			strcmp(entry->name, "..") != 0
		) {
			char subpath[MAX_PATH_LEN];
			snprintf(subpath, MAX_PATH_LEN, "%s/%s", path, entry->name);
			add_dir(subpath, dest, state);
		}
		else if (entry->is_file) {
			char subpath[MAX_PATH_LEN];
			snprintf(subpath, MAX_PATH_LEN, "%s/%s", path, entry->name);
			add_file(subpath, dest, state);
		}
	}
	pi_dir_close(dir);
}

void pack(const char *read_dir, char **sources, int sources_len, const char *archive_path) {
	FILE *dest = fopen(archive_path, "wb");
	error_if(!dest, "Could not open file %s for writing", archive_path);

	pack_state state = {
		.files = malloc(sizeof(qop_file) * 1024),
		.len = 0,
		.capacity = 1024,
		.size = 0
	};

	if (read_dir) {
		error_if(chdir(read_dir) != 0, "Could not change to directory %s", read_dir);
	}

	// Add files/directories
	for (int i = 0; i < sources_len; i++) {
		struct stat s;
		error_if(stat(sources[i], &s) != 0, "Could not stat file %s", sources[i]);
		if (S_ISDIR(s.st_mode)) {
			add_dir(sources[i], dest, &state);
		}
		else if (S_ISREG(s.st_mode)) {
			add_file(sources[i], dest, &state);
		}
		else {
			die("Path %s is neither a directory nor a regular file", sources[i]);
		}
	}

	// Write index and header
	unsigned int total_size = state.size + QOP_HEADER_SIZE;
	for (int i = 0; i < state.len; i++) {
		write_64(state.files[i].hash, dest);
		write_32(state.files[i].offset, dest);
		write_32(state.files[i].size, dest);
		write_16(state.files[i].path_len, dest);
		write_16(state.files[i].flags, dest);
		total_size += 20;
	}

	write_32(state.len, dest);
	write_32(total_size, dest);
	write_32(QOP_MAGIC, dest);

	free(state.files);
	fclose(dest);

	printf("files: %d, size: %d bytes\n", state.len, total_size);
}

void exit_usage(void) {
	puts(
		"Usage: qopconv [OPTION...] FILE...\n"
		"\n"
		"Examples:\n"
		"  qopconv dir1 archive.qop          # Create archive.qop from dir1/\n"
		"  qopconv foo bar archive.qop       # Create archive.qop from files foo and bar\n"
		"  qoponvv -u archive.qop            # Unpack archive.qop in current directory\n"
		"  qopconv -l archive.qop            # List files in archive.qop\n"
		"  qopconv -d dir1 dir2 archive.qop  # Use dir1 prefix for reading, create\n"
		"                                      archive.qop from files in dir1/dir2/\n"
		"\n"
		"Options (mutually exclusive):\n"
		"  -u <archive> ... unpack archive\n"
		"  -l <archive> ... list contents of archive\n"
		"  -d <dir> ....... change read dir when creating archives\n"
	);
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		exit_usage();
	}

	// Unpack
	if (strcmp(argv[1], "-u") == 0) {
		unpack(argv[2], 0);	
	}
	else if (strcmp(argv[1], "-l") == 0) {
		unpack(argv[2], 1);
	}
	else {
		int files_start = 1;
		char *read_dir = NULL;
		if (strcmp(argv[1], "-d") == 0) {
			read_dir = argv[2];
			files_start = 3;
		}
		if (argc < 2 + files_start) {
			exit_usage();
		}
		pack(read_dir, argv + files_start, argc - 1 - files_start, argv[argc-1]);
	}
	return 0;
}
