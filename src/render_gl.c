#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"

#if !defined(RENDER_ATLAS_SIZE)
	#define RENDER_ATLAS_SIZE 64
#endif

#if !defined(RENDER_ATLAS_GRID)
	#define RENDER_ATLAS_GRID 32
#endif

#if !defined(RENDER_ATLAS_BORDER)
	#define RENDER_ATLAS_BORDER 0
#endif

#define RENDER_ATLAS_SIZE_PX (RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID)

#if !defined(RENDER_BUFFER_CAPACITY)
	#define RENDER_BUFFER_CAPACITY 2048
#endif

#if !defined(RENDER_USE_MIPMAPS)
	#define RENDER_USE_MIPMAPS 0
#endif


// -----------------------------------------------------------------------------
// Load OpenGL. This needs to be done differently, depending on the OS.

#if defined(__EMSCRIPTEN__)
	// Emscripten: no loader needed
	#include <GLES3/gl3.h>
#elif defined(__APPLE__) && defined(__MACH__)
	// macOS: no loader needed
	#include <OpenGL/gl3.h>
#elif defined(__unix__)
	// Linux: no loader needed, but we need to explicitly tell gl.h to set up
	// the function prototypes
	#define GL_GLEXT_PROTOTYPES
	#include <GL/gl.h>
#else
	// Windows: use glad to load OpenGL; just embed the source here
	#define RENDER_HAS_GLAD
	#include "../libs/glad.c"
#endif


// -----------------------------------------------------------------------------
// Shader compilation

#define use_program(SHADER) \
	glUseProgram((SHADER)->program); \
	glBindVertexArray((SHADER)->vao);

#define bind_va_f(index, container, member, start) \
	glVertexAttribPointer( \
		index, member_size(container, member)/sizeof(float), GL_FLOAT, false, \
		sizeof(container), \
		(GLvoid*)(offsetof(container, member) + start) \
	)

#define bind_va_color(index, container, member, start) \
	glVertexAttribPointer( \
		index, 4,  GL_UNSIGNED_BYTE, true, \
		sizeof(container), \
		(GLvoid*)(offsetof(container, member) + start) \
	)


static GLuint compile_shader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		int log_written;
		char log[256];
		glGetShaderInfoLog(shader, 256, &log_written, log);
		die("Error compiling shader: %s\nwith source:\n%s", log, source);
	}
	return shader;
}

static GLuint create_program(const char *vs_source, const char *fs_source) {
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_source);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_source);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glUseProgram(program);
	return program;
}

// We try to re-use shaders between GLES2 and GLCORE3. Our requirements are
// very basic, so each vertex or fragment shaders comes with a preamble that
// makes the respective gl version happy.

#if !defined(RENDER_GLSL_VERSION)
	#if defined(__EMSCRIPTEN__) || defined(USE_GLES2)
		#define RENDER_SHADER_PREAMBLE_VS \
			"precision highp float;\n" \
			"#define IN attribute\n" \
			"#define OUT varying\n"
		#define RENDER_SHADER_PREAMBLE_FS \
			"precision highp float;\n" \
			"#define IN varying\n" \
			"#define FRAG_COLOR gl_FragColor\n" \
			"#define OUT_FRAG_COLOR\n"  \
			"#define TEXTURE texture2D\n"
	#else
		#define RENDER_SHADER_PREAMBLE_VS \
			"#version 140\n" \
			"#define IN in\n" \
			"#define OUT out\n"
		#define RENDER_SHADER_PREAMBLE_FS \
			"#version 140\n" \
			"#define IN in\n" \
			"#define FRAG_COLOR fragment_color_output\n" \
			"#define TEXTURE texture\n" \
			"out vec4 FRAG_COLOR;\n"
	#endif
#endif

#define SHADER_SOURCE_VS(...) RENDER_SHADER_PREAMBLE_VS #__VA_ARGS__	
#define SHADER_SOURCE_FS(...) RENDER_SHADER_PREAMBLE_FS #__VA_ARGS__	


// -----------------------------------------------------------------------------
// Main game shaders

static const char * const SHADER_GAME_VS = SHADER_SOURCE_VS(
	IN vec2 pos;
	IN vec2 uv;
	IN vec4 color;
	OUT vec4 v_color;
	OUT vec2 v_uv;

	uniform vec2 screen;
	uniform vec2 fade;
	uniform float time;
	
	void main(void) {
		v_color = color;
		v_uv = uv;
		gl_Position = vec4(
			floor(pos + 0.5) * (vec2(2,-2)/screen.xy) + vec2(-1.0,1.0),
			0.0, 1.0
		);
	}
);

static const char * const SHADER_GAME_FS = SHADER_SOURCE_FS(
	IN vec4 v_color;
	IN vec2 v_uv;

	uniform sampler2D atlas;

	void main(void) {
		vec4 tex_color = TEXTURE(atlas, v_uv);
		vec4 color = tex_color * v_color;
		FRAG_COLOR = color;
	}
);

typedef struct {
	GLuint program;
	GLuint vao;
	struct {
		GLuint screen;
		GLuint time;
	} uniform;
	struct {
		GLuint pos;
		GLuint uv;
		GLuint color;
	} attribute;
} prg_game_t;

prg_game_t *shader_game_init(void) {
	prg_game_t *s = bump_alloc(sizeof(prg_game_t));
	
	s->program = create_program(SHADER_GAME_VS, SHADER_GAME_FS);
	s->uniform.screen = glGetUniformLocation(s->program, "screen");

	s->attribute.pos = glGetAttribLocation(s->program, "pos");
	s->attribute.uv = glGetAttribLocation(s->program, "uv");
	s->attribute.color = glGetAttribLocation(s->program, "color");

	glGenVertexArrays(1, &s->vao);
	glBindVertexArray(s->vao);

	glEnableVertexAttribArray(s->attribute.pos);
	glEnableVertexAttribArray(s->attribute.uv);
	glEnableVertexAttribArray(s->attribute.color);

	bind_va_f(s->attribute.pos, vertex_t, pos, 0);
	bind_va_f(s->attribute.uv, vertex_t, uv, 0);
	bind_va_color(s->attribute.color, vertex_t, color, 0);

	return s;
}


// -----------------------------------------------------------------------------
// POST Effect shaders

static const char * const SHADER_POST_VS = SHADER_SOURCE_VS(
	IN vec2 pos;
	IN vec2 uv;
	OUT vec2 v_uv;

	uniform vec2 screen;
	uniform float time;
	
	void main(void) {
		gl_Position = vec4(
			pos * (vec2(2,-2)/screen.xy) + vec2(-1.0,1.0),
			0.0, 1.0
		);
		v_uv = uv;
	}
);

static const char * const SHADER_POST_FS_DEFAULT = SHADER_SOURCE_FS(
	IN vec2 v_uv;

	uniform sampler2D screenbuffer;

	void main(void) {
		FRAG_COLOR = TEXTURE(screenbuffer, v_uv);
	}
);

// CRT effect based on https://www.shadertoy.com/view/Ms23DR 
// by https://github.com/mattiasgustavsson/
static const char * const SHADER_POST_FS_CRT = SHADER_SOURCE_FS(
	IN vec2 v_uv;

	uniform float time;
	uniform sampler2D screenbuffer;
	uniform vec2 screen;

	vec2 curve(vec2 uv) {
		uv = (uv - 0.5) * 2.0;
		uv *= 1.1;	
		uv.x *= 1.0 + pow((abs(uv.y) / 5.0), 2.0);
		uv.y *= 1.0 + pow((abs(uv.x) / 4.0), 2.0);
		uv  = (uv / 2.0) + 0.5;
		uv =  uv *0.92 + 0.04;
		return uv;
	}

	void main(){
		vec2 uv = curve(v_uv);
		vec3 color;
		float x = 
			sin(0.3 * time + uv.y * 21.0) * sin(0.7 * time + uv.y * 29.0) *
			sin(0.3 + 0.33 * time + uv.y * 31.0) * 0.0017;

		color.r = TEXTURE(screenbuffer, vec2(x + uv.x + 0.001, uv.y + 0.001)).x + 0.05;
		color.g = TEXTURE(screenbuffer, vec2(x + uv.x + 0.000, uv.y - 0.002)).y + 0.05;
		color.b = TEXTURE(screenbuffer, vec2(x + uv.x - 0.002, uv.y + 0.000)).z + 0.05;
		color.r += 0.08 * TEXTURE(screenbuffer, 0.75 * vec2(x + 0.025, -0.027) + vec2(uv.x + 0.001, uv.y + 0.001)).x;
		color.g += 0.05 * TEXTURE(screenbuffer, 0.75 * vec2(x - 0.022, -0.020) + vec2(uv.x + 0.000, uv.y - 0.002)).y;
		color.b += 0.08 * TEXTURE(screenbuffer, 0.75 * vec2(x + -0.02, -0.018) + vec2(uv.x - 0.002, uv.y + 0.000)).z;

		color = clamp(color * 0.6 + 0.4 * color * color * 1.0, 0.0, 1.0);

		float vignette = (0.0 + 1.0 * 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y));
		color *= vec3(pow(vignette, 0.25));
		color *= vec3(0.95,1.05,0.95);
		color *= 2.8;

		float scanlines = clamp( 0.35 + 0.35 * sin(3.5 * time + uv.y * screen.y * 1.5), 0.0, 1.0);
		float s = pow(scanlines,1.7);
		color = color * vec3(0.4 + 0.7 * s);

		color *= 1.0 + 0.01 * sin(110.0 * time);
		if (uv.x < 0.0 || uv.x > 1.0) {
			color *= 0.0;
		}
		if (uv.y < 0.0 || uv.y > 1.0) {
			color *= 0.0;
		}
		
		color *= 1.0 - 0.65 * vec3(clamp((mod(gl_FragCoord.x, 2.0) - 1.0) * 2.0, 0.0, 1.0));
		FRAG_COLOR = vec4(color, 1.0);
	}
);

typedef struct {
	GLuint program;
	GLuint vao;
	struct {
		GLuint screen;
		GLuint time;
	} uniform;
	struct {
		GLuint pos;
		GLuint uv;
	} attribute;
} prg_post_t;

void shader_post_general_init(prg_post_t *s) {
	s->uniform.screen = glGetUniformLocation(s->program, "screen");
	s->uniform.time = glGetUniformLocation(s->program, "time");

	s->attribute.pos = glGetAttribLocation(s->program, "pos");
	s->attribute.uv = glGetAttribLocation(s->program, "uv");

	glGenVertexArrays(1, &s->vao);
	glBindVertexArray(s->vao);

	glEnableVertexAttribArray(s->attribute.pos);
	glEnableVertexAttribArray(s->attribute.uv);

	bind_va_f(s->attribute.pos, vertex_t, pos, 0);
	bind_va_f(s->attribute.uv, vertex_t, uv, 0);
}

prg_post_t *shader_post_default_init(void) {
	prg_post_t *s = bump_alloc(sizeof(prg_post_t));
	s->program = create_program(SHADER_POST_VS, SHADER_POST_FS_DEFAULT);	
	shader_post_general_init(s);
	return s;
}

prg_post_t *shader_post_crt_init(void) {
	prg_post_t *s = bump_alloc(sizeof(prg_post_t));
	s->program = create_program(SHADER_POST_VS, SHADER_POST_FS_CRT);	
	shader_post_general_init(s);
	return s;
}



// -----------------------------------------------------------------------------
// Rendering

typedef struct {
	vec2i_t offset;
	vec2i_t size;
} atlas_pos_t;

texture_t RENDER_NO_TEXTURE;

static GLuint vbo_quads;
static GLuint vbo_indices;

static quadverts_t quad_buffer[RENDER_BUFFER_CAPACITY];
static uint16_t index_buffer[RENDER_BUFFER_CAPACITY][6];
static uint32_t quad_buffer_len = 0;

static vec2i_t screen_size;
static vec2i_t backbuffer_size;

static uint32_t atlas_map[RENDER_ATLAS_SIZE] = {0};
static GLuint atlas_texture = 0;
static render_blend_mode_t blend_mode = RENDER_BLEND_NORMAL;

static atlas_pos_t textures[RENDER_TEXTURES_MAX];
static uint32_t textures_len = 0;
static bool mipmap_is_dirty = false;

static GLuint backbuffer = 0;
static GLuint backbuffer_texture = 0;

prg_game_t *prg_game;
prg_post_t *prg_post;
prg_post_t *prg_post_effects[RENDER_POST_MAX] = {};



static void render_flush(void);


// static void gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei len, const GLchar *message, const void *userParam) {
// 	printf("GL: %s\n", message);
// }

void render_backend_init(void) {
	#if defined(RENDER_HAS_GLAD)
		gladLoadGL();
	#endif

	// glEnable(GL_DEBUG_OUTPUT);
	// glDebugMessageCallback(gl_message_callback, NULL);
	// glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);


	// Atlas Texture

	glGenTextures(1, &atlas_texture);
	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, RENDER_USE_MIPMAPS ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	uint32_t tw = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
	uint32_t th = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	

	// Quad buffer

	glGenBuffers(1, &vbo_quads);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_quads);

	// Index buffer

	glGenBuffers(1, &vbo_indices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);

	for (uint32_t i = 0, j = 0; i < RENDER_BUFFER_CAPACITY; i++, j += 4) {
		index_buffer[i][0] = j + 3;
		index_buffer[i][1] = j + 1;
		index_buffer[i][2] = j + 0;
		index_buffer[i][3] = j + 3;
		index_buffer[i][4] = j + 2;
		index_buffer[i][5] = j + 1;
	}
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer), index_buffer, GL_STATIC_DRAW);




	// Post Shaders

	prg_post_effects[RENDER_POST_NONE] = shader_post_default_init();
	prg_post_effects[RENDER_POST_CRT] = shader_post_crt_init();
	render_set_post_effect(RENDER_POST_NONE);

	// Game shader

	prg_game = shader_game_init();
	use_program(prg_game);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Create white texture

	rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
	RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);
}

void render_backend_cleanup(void) {
	// TODO
}

void render_set_screen(vec2i_t size) {
	screen_size = size;
	backbuffer_size = screen_size;

	if (!backbuffer) {
		glGenTextures(1, &backbuffer_texture);	
		glGenFramebuffers(1, &backbuffer);
	}
	
	glBindTexture(GL_TEXTURE_2D, backbuffer_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, backbuffer_size.x, backbuffer_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
	glBindFramebuffer(GL_FRAMEBUFFER, backbuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backbuffer_texture, 0);

	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, RENDER_USE_MIPMAPS ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glViewport(0, 0, backbuffer_size.x, backbuffer_size.y);
}

void render_set_post_effect(render_post_effect_t post) {
	error_if(post < 0 || post > RENDER_POST_MAX, "Invalid post effect %d", post);
	prg_post = prg_post_effects[post];
}

void render_frame_prepare(void) {
	use_program(prg_game);
	glBindFramebuffer(GL_FRAMEBUFFER, backbuffer);
	glViewport(0, 0, backbuffer_size.x, backbuffer_size.y);

	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glUniform2f(prg_game->uniform.screen, backbuffer_size.x, backbuffer_size.y);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST); 
}

void render_frame_end(void) {
	render_flush();

	// Draw backbuffer to screen
	use_program(prg_post);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screen_size.x, screen_size.y);
	glBindTexture(GL_TEXTURE_2D, backbuffer_texture);
	glUniform1f(prg_post->uniform.time, engine.time);
	glUniform2f(prg_post->uniform.screen, screen_size.x, screen_size.y);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	quad_buffer[quad_buffer_len] = (quadverts_t){
		.vertices = {
			{.pos = {0,             0            }, .uv = {0, 1}, .color = rgba_white()},
			{.pos = {screen_size.x, 0            }, .uv = {1, 1}, .color = rgba_white()},
			{.pos = {screen_size.x, screen_size.y}, .uv = {1, 0}, .color = rgba_white()},
			{.pos = {0,             screen_size.y}, .uv = {0, 0}, .color = rgba_white()},
		}
	};
	quad_buffer_len++;
	render_flush();
}

void render_flush(void) {
	if (mipmap_is_dirty) {
		glGenerateMipmap(GL_TEXTURE_2D);
		mipmap_is_dirty = false;
	}

	if (quad_buffer_len == 0) {
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo_quads);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadverts_t) * quad_buffer_len, quad_buffer, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices);
	glDrawElements(GL_TRIANGLES, quad_buffer_len * 6, GL_UNSIGNED_SHORT, 0);
	quad_buffer_len = 0;
}

void render_set_blend_mode(render_blend_mode_t new_mode) {
	if (new_mode == blend_mode) {
		return;
	}
	render_flush();

	blend_mode = new_mode;
	if (blend_mode == RENDER_BLEND_NORMAL) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (blend_mode == RENDER_BLEND_LIGHTER) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
}

void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);
	atlas_pos_t *t = &textures[texture_handle.index];

	if (quad_buffer_len >= RENDER_BUFFER_CAPACITY) {
		render_flush();
	}

	quad_buffer[quad_buffer_len] = *quad;
	for (uint32_t i = 0; i < 4; i++) {
		quad_buffer[quad_buffer_len].vertices[i].uv.x = 
			(quad_buffer[quad_buffer_len].vertices[i].uv.x + t->offset.x) * (1.0 / RENDER_ATLAS_SIZE_PX);
		quad_buffer[quad_buffer_len].vertices[i].uv.y = 
			(quad_buffer[quad_buffer_len].vertices[i].uv.y + t->offset.y) * (1.0 / RENDER_ATLAS_SIZE_PX);
	}
	quad_buffer_len++;
}



// -----------------------------------------------------------------------------
// Textures

texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textures_len};
}

void textures_reset(texture_mark_t mark) {
	error_if(mark.index > textures_len, "Invalid texture reset mark %d >= %d", mark.index, textures_len);
	if (mark.index == textures_len) {
		return;
	}
	
	render_flush();

	textures_len = mark.index;
	clear(atlas_map);

	// Clear completely and recreate the default white texture
	if (textures_len == 0) {
		rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
		RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);
		return;
	}

	// Replay all texture grid insertions up to the reset len
	for (int i = 0; i < textures_len; i++) {
		uint32_t grid_x = (textures[i].offset.x - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_y = (textures[i].offset.y - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_width = (textures[i].size.x + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		uint32_t grid_height = (textures[i].size.y + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
			atlas_map[cx] = grid_y + grid_height;
		}
	}
}

texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	error_if(textures_len >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");

	uint32_t bw = size.x + RENDER_ATLAS_BORDER * 2;
	uint32_t bh = size.y + RENDER_ATLAS_BORDER * 2;

	// Find a position in the atlas for this texture (with added border)
	uint32_t grid_width = (bw + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
	uint32_t grid_height = (bh + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
	uint32_t grid_x = 0;
	uint32_t grid_y = RENDER_ATLAS_SIZE - grid_height + 1;

	error_if(grid_width > RENDER_ATLAS_SIZE || grid_height > RENDER_ATLAS_SIZE, "Texture of size %dx%d doesn't fit in atlas", size.x, size.y);

	for (uint32_t cx = 0; cx < RENDER_ATLAS_SIZE - grid_width; cx++) {
		if (atlas_map[cx] >= grid_y) {
			continue;
		}

		uint32_t cy = atlas_map[cx];
		bool is_best = true;

		for (uint32_t bx = cx; bx < cx + grid_width; bx++) {
			if (atlas_map[bx] >= grid_y) {
				is_best = false;
				cx = bx;
				break;
			}
			if (atlas_map[bx] > cy) {
				cy = atlas_map[bx];
			}
		}
		if (is_best) {
			grid_y = cy;
			grid_x = cx;
		}
	}

	error_if(grid_y + grid_height > RENDER_ATLAS_SIZE, "Render atlas ran out of space for %dx%d texture", size.x, size.y);

	for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
		atlas_map[cx] = grid_y + grid_height;
	}

	uint32_t x = grid_x * RENDER_ATLAS_GRID;
	uint32_t y = grid_y * RENDER_ATLAS_GRID;
	glBindTexture(GL_TEXTURE_2D, atlas_texture);

	// Add the border pixels for this texture
	#if RENDER_ATLAS_BORDER > 0
		rgba_t *pb = temp_alloc(sizeof(rgba_t) * bw * bh);

		if (size.x && size.y) {
			// Top border
			for (int32_t y = 0; y < RENDER_ATLAS_BORDER; y++) {
				memcpy(pb + bw * y + RENDER_ATLAS_BORDER, pixels, size.x * sizeof(rgba_t));
			}

			// Bottom border
			for (int32_t y = 0; y < RENDER_ATLAS_BORDER; y++) {
				memcpy(pb + bw * (bh - RENDER_ATLAS_BORDER + y) + RENDER_ATLAS_BORDER, pixels + size.x * (size.y-1), size.x * sizeof(rgba_t));
			}
			
			// Left border
			for (int32_t y = 0; y < bh; y++) {
				for (int32_t x = 0; x < RENDER_ATLAS_BORDER; x++) {
					pb[y * bw + x] = pixels[clamp(y-RENDER_ATLAS_BORDER, 0, size.y-1) * size.x];
				}
			}

			// Right border
			for (int32_t y = 0; y < bh; y++) {
				for (int32_t x = 0; x < RENDER_ATLAS_BORDER; x++) {
					pb[y * bw + x + bw - RENDER_ATLAS_BORDER] = pixels[size.x - 1 + clamp(y-RENDER_ATLAS_BORDER, 0, size.y-1) * size.x];
				}
			}

			// Texture
			for (int32_t y = 0; y < size.y; y++) {
				memcpy(pb + bw * (y + RENDER_ATLAS_BORDER) + RENDER_ATLAS_BORDER, pixels + size.x * y, size.x * sizeof(rgba_t));
			}
		}

		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, pb);
		temp_free(pb);
	#else
		glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, bw, bh, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	#endif

	mipmap_is_dirty = RENDER_USE_MIPMAPS;
	texture_t texture_handle = {.index = textures_len};
	textures_len++;
	textures[texture_handle.index] = (atlas_pos_t){.offset = {x + RENDER_ATLAS_BORDER, y + RENDER_ATLAS_BORDER}, .size = size};
	
	return texture_handle;
}

void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	error_if(texture_handle.index >= textures_len, "Invalid texture %d", texture_handle.index);

	atlas_pos_t *t = &textures[texture_handle.index];
	error_if(t->size.x < size.x || t->size.y < size.y, "Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, t->size.x, t->size.y);

	glBindTexture(GL_TEXTURE_2D, atlas_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, t->offset.x, t->offset.y, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

// void textures_dump(const char *path) {
// 	int width = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
// 	int height = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
// 	rgba_t *pixels = malloc(sizeof(rgba_t) * width * height);
// 	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
// 	stbi_write_png(path, width, height, 4, pixels, 0);
// 	free(pixels);
// }
