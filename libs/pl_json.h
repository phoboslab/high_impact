/*

Copyright (c) 2024, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


PL_JSON - Yet another single header json parser


-- About

PL_JSON decodes JSON into a structure that is easy to use. 

This library does not check the JSON data for conformance. Escape sequences 
such as '\n', '\r', or '\\' are handled, but unicode escape sequences (\uffff)
are not - these will be replaced by a single '?' character. So use JSON that
has utf8 strings instead of unicode escape sequences.

The caller is responsible for memory allocation. To facilitate memory efficient 
use, the parsing is split into two steps: 
	1) tokenization into a temporary token buffer
	2) parsing of the tokens into the final structure


-- Synopsis:

// Define `PL_JSON_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"

// Read file
FILE *fh = fopen(path, "rb");
fseek(fh, 0, SEEK_END);
int size = ftell(fh);
fseek(fh, 0, SEEK_SET);

char *json_text = malloc(size);
fread(json_text, 1, size, fh);
fclose(fh);

// Tokenize JSON. Allocate some temporary memory that is big enough to hold the 
// expected number of tokens. The maximum number of tokens for a given byte size
// can be no larger than 1 + size / 2 
// E.g. for the worst case `[0,0,0]` = 7 bytes, 4 tokens
unsigned int tokens_capacity = 1 + size / 2;
json_token_t *tokens = malloc(tokens_capacity * sizeof(json_token_t));

unsigned int size_req;
unsigned int tokens_len = json_tokenize(json_text, size, tokens, tokens_capacity, &size_req);
assert(tokens_len > 0);

// Parse tokens. Allocate memory for the final json object. The size_req 
// returned by json_tokenize is always sufficient and close to optimal.
json_t *v = malloc(size_req);
json_parse_tokens(json_text, tokens, tokens_len, v);

// The json_text and tokens are no longer required and can be freed.
free(json_text);
free(tokens);

// Assuming the input `[1, 2, 3, "hello", 4, "world"]`, print the array of 
// numbers and strings:

if (v->type == JSON_ARRAY) {
	json_t *values = v->data;
	for (int i = 0; i < v->len; i++) {
		if (values[i].type == JSON_STRING) {
			printf("string with length %d: %s\n", values[i].len, values[i].string);
		}
		else if (values[i].type == JSON_NUMBER) {
			printf("number: %f\n", values[i].number);	
		}
	}
}

// Some convenience functions can be used to access the data of json_t; these
// functions are null-safe and check the underlying type. So doing something 
// like this is always fine, as long as you check the final return value:

char *str = json_string(json_value_for_key(v, "foobar"));
if (str != null) {
	printf("string value for key foobar: %s", str);
}

// See below for the documentation of these functions.

// Finally, when you're done with the parsed json, just free it again.
free(v);

*/



/* -----------------------------------------------------------------------------
	Header - Public functions */

#ifndef PL_JSON_H
#define PL_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	JSON_NULL,
	JSON_TRUE,
	JSON_FALSE,
	JSON_NUMBER,
	JSON_STRING,
	JSON_ARRAY,
	JSON_OBJECT
} json_type_t;

typedef struct {
	json_type_t type;
	unsigned int pos;
	unsigned int len;
} json_token_t;

typedef struct json_t {
	json_type_t type;
	unsigned int len;
	union {
		double number;
		char *string;
		struct json_t *values;
	};
} json_t;

typedef enum {
	JSON_ERROR_INVALID    = -1,
	JSON_ERROR_MAX_TOKENS = -2,
	JSON_ERROR_MAX_DEPTH  = -3,
} json_error_t;


// Tokenize JSON data
// - len denotes the length of the input JSON. If data is a null terminated 
//   string and the length is not know, it is safe to set len to 0xffffffff.
// - tokens is a pointer to a user allocated array of tokens of tokens_len.
// - parsed_size_req will be set to the number of bytes required for json_parse_tokens
// Returns the number of tokens parsed, or a negative value corresponding to
// the json_error_t enum if the input is invalid or the given tokens_len was not 
// sufficient.

int json_tokenize(char *data, unsigned int len, json_token_t *tokens, unsigned int tokens_len, unsigned int *parsed_size_req);


// Parse tokens into json_t
// - data MUST be the same data as for json_tokenize
// - len is the number of tokens to parse (i.e. the return value from json_tokenize)
// - json is a pointer to user allocated memory of sufficient size, ie. the
//   parsed_size_req from json_tokenize

void json_parse_tokens(char *data, json_token_t *tokens, unsigned int len, json_t *json);


// The actual number for NUMBER, 1 for TRUE, 0 for all other types.
double json_number(json_t *v);

// 1 for truthy values (TRUE, non-zero NUMBER, non-empty STRING, 
// non-empty OBJECT or non-empty ARRAY) or 0 for all others.
int json_bool(json_t *v);

// A pointer to a NULL terminated string for STRING or NULL
char *json_string(json_t *v);

// A pointer to an array of v->len of json_t for ARRAY or OBJECT or NULL
json_t *json_values(json_t *v);

// A pointer to the i-th value of an ARRAY or OBJECT or NULL
json_t *json_value_at(json_t *v, unsigned int i);

// A pointer to an array of v->len of pointers to NULL terminated strings 
// for OBJECT or NULL
char **json_keys(json_t *v);

// A NULL terminated string for the i-th key for OBJECT or NULL
char *json_key_at(json_t *v, unsigned int i);

// A pointer to the value for the key for OBJECT or NULL. Uses linear search, so
// be mindful about performance with objects with more than a few hundred keys.
json_t *json_value_for_key(json_t *v, char *key);

#ifdef __cplusplus
}
#endif
#endif /* PL_JSON_H */



/* -----------------------------------------------------------------------------
	Implementation */

#ifdef PL_JSON_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

#define JSON_MAX_DEPTH 256

typedef struct {
	json_error_t error;
	char *data;
	unsigned int data_len;
	unsigned int pos;
	json_token_t *tokens;
	unsigned int tokens_capacity;
	unsigned int tokens_len;
	unsigned int tokens_pos;
	unsigned int parsed_size;
	unsigned char *parsed_data;
	unsigned int parsed_data_len;
} json_parser_t;

enum {
	JSON_C_NULL  = (1<<0),
	JSON_C_SPACE = (1<<1),
	JSON_C_LF    = (1<<2),
	JSON_C_NUM   = (1<<3),
	JSON_C_EXP   = (1<<5),
	JSON_C_PRIM  = (1<<6),
	JSON_C_OBJ   = (1<<7)
};
static const unsigned char json_char_map[256] = {
	['\0'] = JSON_C_NULL,  ['\t'] = JSON_C_SPACE, ['\n'] = JSON_C_LF,
	['\r'] = JSON_C_SPACE, [' ']  = JSON_C_SPACE, ['+']  = JSON_C_EXP,
	['-']  = JSON_C_NUM,   ['.']  = JSON_C_EXP,   ['0']  = JSON_C_NUM,
	['1']  = JSON_C_NUM,   ['2']  = JSON_C_NUM,   ['3']  = JSON_C_NUM,
	['4']  = JSON_C_NUM,   ['5']  = JSON_C_NUM,   ['6']  = JSON_C_NUM,
	['7']  = JSON_C_NUM,   ['8']  = JSON_C_NUM,   ['9']  = JSON_C_NUM,
	['n']  = JSON_C_PRIM,  ['t']  = JSON_C_PRIM,  ['f']  = JSON_C_PRIM,
	['"']  = JSON_C_OBJ,   ['[']  = JSON_C_OBJ,   ['{']  = JSON_C_OBJ,
	['E']  = JSON_C_EXP,   ['e']  = JSON_C_EXP,
};

#define json_char_is(C, TYPE) (json_char_map[(unsigned char)(C)] & (TYPE))

static inline char json_next(json_parser_t *parser) {
	if (parser->pos >= parser->data_len) {
		return '\0';
	}
	return parser->data[parser->pos++];
}

static inline char json_next_non_whitespace(json_parser_t *parser) {
	char c;
	do {
		c = json_next(parser);
	} while (json_char_is(c, JSON_C_SPACE | JSON_C_LF));
	return c;
}

static json_token_t *json_tokenize_descent(json_parser_t *parser, unsigned int depth) {
	if (depth > JSON_MAX_DEPTH) {
		parser->error = JSON_ERROR_MAX_DEPTH;
		return NULL;
	}
	if (parser->tokens_len >= parser->tokens_capacity) {
		parser->error = JSON_ERROR_MAX_TOKENS;
		return NULL;
	}

	parser->parsed_size += sizeof(json_t);
	char c = json_next_non_whitespace(parser);

	json_token_t *token = &parser->tokens[parser->tokens_len++];
	token->pos = parser->pos - 1;
	token->len = 0;

	if (json_char_is(c, JSON_C_OBJ)) {
		if (c == '"') {
			token->type = JSON_STRING;
			token->pos++;
			c = json_next(parser);
			while (c != '"') {
				if (c == '\\') {
					c = json_next(parser);
				}
				else if (json_char_is(c, JSON_C_NULL | JSON_C_LF)) {
					return NULL;
				}
				c = json_next(parser);
			}
			token->len = parser->pos - token->pos - 1;
			parser->parsed_size += token->len + 1;
			return token;
		}
		else if (c == '{') {
			token->type = JSON_OBJECT;
			c = json_next_non_whitespace(parser);
			if (c == '}') {
				return token;
			}
			while (1) {
				parser->pos--;
				token->len++;
				json_token_t *key = json_tokenize_descent(parser, depth+1);
				if (!key || key->type != JSON_STRING) {
					return NULL;
				}
				c = json_next_non_whitespace(parser);
				if (c != ':') {
					return NULL;
				}
				if (!json_tokenize_descent(parser, depth+1)) {
					return NULL;
				}
				c = json_next_non_whitespace(parser);
				if (c == ',') {
					c = json_next_non_whitespace(parser);
				}
				else if (c == '}') {
					return token;
				}
				else {
					return NULL;
				}
			}
		}
		else /* if (c == '[') */ {
			token->type = JSON_ARRAY;
			c = json_next_non_whitespace(parser);
			if (c == ']') {
				return token;
			}
			while (1) {
				parser->pos--;
				token->len++;
				if (!json_tokenize_descent(parser, depth+1)) {
					return NULL;
				}
				c = json_next_non_whitespace(parser);
				if (c == ',') {
					c = json_next_non_whitespace(parser);
				}
				else if (c == ']') {
					return token;
				}
				else {
					return NULL;
				}
			}
		}
	}
	else if (json_char_is(c, JSON_C_PRIM)) {
		if (c == 'n') {
			token->type = JSON_NULL;
			parser->pos += 3;
		}
		else if (c == 't') {
			token->type = JSON_TRUE;
			parser->pos += 3;
		}
		else /* if (c == 'f') */ {
			token->type = JSON_FALSE;
			parser->pos += 4;
		}
		return token;
	}
	else if (json_char_is(c, JSON_C_NUM)) { 
		token->type = JSON_NUMBER;
		do {
			c = json_next(parser);
		} while (json_char_is(c, JSON_C_NUM | JSON_C_EXP));
		parser->pos--;
		token->len = parser->pos - token->pos;
		if (token->len > 63) {
			return NULL;
		}
		return token;
	}

	return NULL;
}

static inline void *json_bump_alloc(json_parser_t *parser, unsigned int size) {
	void *p = parser->parsed_data + parser->parsed_data_len;
	parser->parsed_data_len += size;
	return p;
}

static unsigned int json_parse_string(char *dst, char *src, unsigned int len) {
	unsigned int di = 0;
	for (unsigned int si = 0; si < len; si++, di++) {
		char c = src[si];
		if (c == '\\') {
			c = src[++si];
			switch (c) {
				case 'r': c = '\r'; break;
				case 'n': c = '\n'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				case 't': c = '\t'; break;
				case 'u': c = '?'; si += 4; break;
			}
		}
		dst[di] = c;
	}
	dst[di] = '\0';
	return di;
}

static void json_parse_descent(json_parser_t *parser, json_t *v) {
	json_token_t *t = &parser->tokens[parser->tokens_pos++];
	v->type = t->type;

	if (v->type == JSON_NULL) {
		v->number = 0;
	}
	else if (v->type == JSON_TRUE) {
		v->number = 1;
	}
	else if (v->type == JSON_FALSE) {
		v->number = 0;
	}
	else if (v->type == JSON_NUMBER) {
		char buf[64];
		memcpy((void *)buf, &parser->data[t->pos], t->len);
		buf[t->len] = '\0';
		v->number = atof(buf);
	}
	else if (v->type == JSON_STRING) {
		char *str = json_bump_alloc(parser, t->len + 1);
		v->len = json_parse_string(str, &parser->data[t->pos], t->len);
		v->string = str;
	}
	else if (v->type == JSON_OBJECT) {
		json_t *values = json_bump_alloc(parser, sizeof(json_t) * t->len);
		char **keys = json_bump_alloc(parser, sizeof(char*) * t->len);
		v->values = values; 
		v->len = t->len;

		for (unsigned int i = 0; i < t->len; i++) {
			json_token_t *key = &parser->tokens[parser->tokens_pos++];
			keys[i] = json_bump_alloc(parser, key->len + 1);
			json_parse_string(keys[i], &parser->data[key->pos], key->len);
			json_parse_descent(parser, &values[i]);
		}
	}
	else if (v->type == JSON_ARRAY) {
		json_t *values = json_bump_alloc(parser, sizeof(json_t) * t->len);
		v->len = t->len;
		v->values = values; 
		for (unsigned int i = 0; i < t->len; i++) {
			json_parse_descent(parser, &values[i]);
		}
	}
}



// Public API ------------------------------------------------------------------

int json_tokenize(char *data, unsigned int len, json_token_t *tokens, unsigned int tokens_len, unsigned int *parsed_size_req) {
	json_parser_t parser = {
		.error = JSON_ERROR_INVALID,
		.data = data,
		.data_len = len,
		.tokens = tokens,
		.tokens_capacity = tokens_len
	};
	if (!json_tokenize_descent(&parser, 0)) {
		return parser.error;
	}

	*parsed_size_req = parser.parsed_size;
	return parser.tokens_len;
}

void json_parse_tokens(char *data, json_token_t *tokens, unsigned int tokens_len, json_t *json) {
	json_parser_t parser = {
		.data = data,
		.tokens = tokens,
		.tokens_len = tokens_len,
		.parsed_data = (unsigned char *)json,
	};
	json_parse_descent(&parser, json_bump_alloc(&parser, sizeof(json_t)));
}

double json_number(json_t *v) {
	if (!v || v->type > JSON_NUMBER) {
		return 0;
	}
	return v->number;
}

int json_bool(json_t *v) {
	if (!v) {
		return 0;
	}
	if (v->type > JSON_NUMBER) {
		return (v->len > 0);
	}
	return (v->number != 0);
}

char *json_string(json_t *v) {
	if (!v || v->type != JSON_STRING) {
		return NULL;
	}
	return v->string;
}

json_t *json_values(json_t *v) {
	if (!v || v->len == 0 || v->type < JSON_ARRAY) {
		return NULL;
	}
	return v->values;
}

json_t *json_value_at(json_t *v, unsigned int i) {
	if (!v || i >= v->len || v->type < JSON_ARRAY) {
		return NULL;
	}
	return v->values + i;
}

char **json_keys(json_t *v) {
	if (!v || v->len == 0 || v->type != JSON_OBJECT) {
		return NULL;
	}
	return (char **)(v->values + v->len);
}

char *json_key_at(json_t *v, unsigned int i) {
	if (!v || i >= v->len || v->type != JSON_OBJECT) {
		return NULL;
	}
	return (char *)(((char **)(v->values + v->len)) + i);
}

json_t *json_value_for_key(json_t *v, char *key) {
	if (!v || v->type != JSON_OBJECT) {
		return NULL;
	}
	char **keys = json_keys(v);
	for (unsigned int i = 0; i < v->len; i++) {
		if (strcmp(keys[i], key) == 0) {
			return json_value_at(v, i);
		}
	}
	return NULL;
}



#endif /* PL_JSON_IMPLEMENTATION */
