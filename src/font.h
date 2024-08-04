#ifndef HI_FONT_H
#define HI_FONT_H

// Fonts are a wrapper around image() that makes it easier to draw text on the
// screen. The image must contains all ASCII characters from 32..127 on a single
// line and the width map must be a json file consisting of a single array of 
// 95 numbers that specify the width of each character.

#include "types.h"
#include "image.h"
#include "../libs/pl_json.h"

typedef struct {
	vec2_t pos;
	vec2_t size;
	vec2_t offset;
	int advance;
} font_glyph_t;

typedef struct {
	// The line height when drawing multi-line text. By default, this is the
	// image height * 1.25. Increase this to have more line spacing.
	int line_height;

	// Extra spacing between each letter on a single line. Default 0
	int letter_spacing;

	// A tint color for this font. Default rgba_white() 
	rgba_t color;

	// Internal state
	int first_char;
	int last_char;
	image_t *image;
	font_glyph_t *glyphs;
} font_t;

typedef enum {
	FONT_ALIGN_LEFT,
	FONT_ALIGN_CENTER,
	FONT_ALIGN_RIGHT,
} font_align_t;

// Create a font with the given path to the image and path to the width_map.json
font_t *font(char *path, char *definition_path);

// Return the line width for the given text
int font_line_width(font_t *font, char *text);

// Draw some text. \n starts a new line. pos is the "anchor" position of the
// text, where y is always the top of the character (not the baseline) and x
// is either the left, right or center position according to align.
void font_draw(font_t *font, vec2_t pos, char *text, font_align_t align);

#endif
