#include "font.h"
#include "alloc.h"
#include "platform.h"
#include "utils.h"


static int font_draw_line(font_t *font, vec2_t pos, char *c, font_align_t align);

font_t *font(char *path, char *definition_path) {
	font_t *font = bump_alloc(sizeof(font_t));
	font->image = image(path);
	font->letter_spacing = 0;
	font->color = rgba_white();

	json_t *def = platform_load_asset_json(definition_path);
	error_if(def == NULL, "Couldn't load font definition json");

	json_t *metrics = json_value_for_key(def, "metrics");

	font->first_char = json_number(json_value_for_key(def, "first_char"));
	font->last_char = json_number(json_value_for_key(def, "last_char"));
	font->line_height = json_number(json_value_for_key(def, "height"));

	int expected_chars = font->last_char - font->first_char;

	error_if(metrics == NULL || metrics->type != JSON_ARRAY, "Font metrics are not an array");
	error_if(metrics->len / 7 != expected_chars, "Font metrics has incorrect length (expected %d have %d)", expected_chars, metrics->len / 7);

	font->glyphs = bump_alloc(expected_chars * sizeof(font_glyph_t));
	for (int i = 0, a = 0; i < expected_chars; i++, a += 7) {
		font->glyphs[i] = (font_glyph_t){
			.pos    = {metrics->values[a + 0].number, metrics->values[a + 1].number},
			.size   = {metrics->values[a + 2].number, metrics->values[a + 3].number},
			.offset = {metrics->values[a + 4].number, metrics->values[a + 5].number},
			.advance = metrics->values[a + 6].number
		};
	}

	temp_free(def);
	return font;
}

void font_draw(font_t *font, vec2_t pos, char *text, font_align_t align) {
	char *c = text;

	while (*c != '\0') {
		while (*c == '\n') {
			pos.y += font->line_height;
			c++;
		}
		c += font_draw_line(font, pos, c, align);
	}
}

int font_line_width(font_t *font, char *text) {
	int width = 0;
	for (char *c = text; *c != '\0' && *c != '\n'; c++) {
		if (*c >= font->first_char && *c <= font->last_char) {
			width += font->glyphs[*c - font->first_char].advance + font->letter_spacing;
		}
	}
	return max(0, width - font->letter_spacing);
}

static int font_draw_line(font_t *font, vec2_t pos, char *text, font_align_t align) {
	if (align == FONT_ALIGN_CENTER || align == FONT_ALIGN_RIGHT) {
		int width = font_line_width(font, text);
		pos.x -= align == FONT_ALIGN_CENTER ? width / 2 : width;
	}

	vec2_t glyph_offset = vec2(0, 0);
	vec2_t glyph_size = vec2(0, image_size(font->image).y);
	int char_count = 0;
	for (char *c = text; *c != '\0' && *c != '\n'; c++, char_count++) {
		if (*c >= font->first_char && *c <= font->last_char) {
			font_glyph_t *g = &font->glyphs[(int)(*c) - font->first_char];
			image_draw_ex(font->image, g->pos, g->size, vec2_add(pos, g->offset), g->size, font->color);
			pos.x += g->advance + font->letter_spacing;
		}
	}

	return char_count;
}