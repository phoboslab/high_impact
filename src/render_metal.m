#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"
#include "platform.h"

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

#if !defined(__OBJC__)
	#error Metal renderer must be compiled in Objective-C mode (-x objective-c).
#endif

#if !__has_feature(objc_arc)
	#error Metal renderer requires Objective-C ARC (-fobjc-arc).
#endif

#if defined(__APPLE__)
	#import <CoreGraphics/CoreGraphics.h>
	#import <Metal/Metal.h>
	#import <QuartzCore/CAMetalLayer.h>
	#include <simd/simd.h>
#else
	#error Compiling Metal renderer for non-Apple platform is not supported.
#endif

// -----------------------------------------------------------------------------
// Shaders

static NSString *const shaderSource = @""
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float2 screen;\n"
"    float2 fade;\n"
"    float time;\n"
"};\n"
"\n"
"struct Vin {\n"
"    packed_float2 pos;\n"
"    packed_float2 uv;\n"
"    uint color;\n"
"};\n"
"\n"
"struct Vout {\n"
"    float4 pos [[position]];\n"
"    float4 color;\n"
"    float2 uv;\n"
"};\n"
"\n"
"vertex Vout vertex_main(device Vin const* vertices [[buffer(0)]],\n"
"                        constant Uniforms &u [[buffer(1)]],\n"
"                        uint vertexID [[vertex_id]])\n"
"{\n"
"    Vin in = vertices[vertexID];\n"
"    Vout out {\n"
"        .pos = float4(\n"
"            floor(in.pos + 0.5f) * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f\n"
"        ),\n"
"        .color = unpack_unorm4x8_to_float(in.color),\n"
"        .uv = in.uv\n"
"    };\n"
"    out.pos.y *= -1.0f;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 fragment_main(Vout in [[stage_in]],\n"
"                              texture2d<float, access::sample> atlas [[texture(0)]],\n"
"                              sampler atlasSampler [[sampler(0)]])\n"
"{\n"
"    float4 tex_color = atlas.sample(atlasSampler, in.uv);\n"
"    float4 color = tex_color * in.color;\n"
"    return color;\n"
"}\n"
"\n"
"struct VPout {\n"
"    float4 pos [[position]];\n"
"    float2 uv;\n"
"};\n"
"\n"
"vertex VPout vertex_post(device Vin const* vertices [[buffer(0)]],\n"
"                         constant Uniforms &u [[buffer(1)]],\n"
"                         uint vertexID [[vertex_id]])\n"
"{\n"
"    Vin in = vertices[vertexID];\n"
"    VPout out {\n"
"        .pos = float4(in.pos * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f),\n"
"        .uv = in.uv\n"
"    };\n"
"    out.pos.y *= -1.0f;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 fragment_post_default(VPout in [[stage_in]],\n"
"                                      texture2d<float, access::sample> screenbuffer [[texture(0)]],\n"
"                                      sampler screenSampler [[sampler(0)]])\n"
"{\n"
"    return screenbuffer.sample(screenSampler, in.uv);\n"
"}\n"
"\n"
"// CRT effect based on https://www.shadertoy.com/view/Ms23DR\n"
"// by https://github.com/mattiasgustavsson/\n"
"\n"
"static float2 curve(float2 uv) {\n"
"    uv = (uv - 0.5f) * 2.0f;\n"
"    uv *= 1.1f;\n"
"    uv.x *= 1.0f + powr((abs(uv.y) / 5.0f), 2.0f);\n"
"    uv.y *= 1.0f + powr((abs(uv.x) / 4.0f), 2.0f);\n"
"    uv  = (uv / 2.0f) + 0.5f;\n"
"    uv =  uv * 0.92f + 0.04f;\n"
"    return uv;\n"
"}\n"
"\n"
"fragment float4 fragment_post_crt(VPout in [[stage_in]],\n"
"                                  constant Uniforms &u [[buffer(1)]],\n"
"                                  texture2d<float, access::sample> screenbuffer [[texture(0)]],\n"
"                                  sampler screenSampler [[sampler(0)]])\n"
"{\n"
"    float2 uv = curve(in.uv);\n"
"    float3 color;\n"
"    float x =\n"
"        sin(0.3f * u.time + in.uv.y * 21.0f) * sin(0.7f * u.time + uv.y * 29.0f) *\n"
"        sin(0.3f + 0.33f * u.time + uv.y * 31.0f) * 0.0017f;\n"
"\n"
"    color.r = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.001f, uv.y + 0.001f)).x + 0.05f;\n"
"    color.g = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.000f, uv.y - 0.002f)).y + 0.05f;\n"
"    color.b = screenbuffer.sample(screenSampler, float2(x + uv.x - 0.002f, uv.y + 0.000f)).z + 0.05f;\n"
"    color.r += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + 0.025f, -0.027f) + float2(uv.x + 0.001f, uv.y + 0.001f)).x;\n"
"    color.g += 0.05 * screenbuffer.sample(screenSampler, 0.75f * float2(x - 0.022f, -0.020f) + float2(uv.x + 0.000f, uv.y - 0.002f)).y;\n"
"    color.b += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + -0.02f, -0.018f) + float2(uv.x - 0.002f, uv.y + 0.000f)).z;\n"
"\n"
"    color = saturate(color * 0.6f + 0.4f * color * color * 1.0f);\n"
"\n"
"    float vignette = (0.0f + 1.0f * 16.0f * uv.x * uv.y * (1.0f - uv.x) * (1.0f - uv.y));\n"
"    color *= float3(powr(vignette, 0.25f));\n"
"    color *= float3(0.95f, 1.05f, 0.95f);\n"
"    color *= 2.8;\n"
"\n"
"    float scanlines = saturate(0.35f + 0.35f * sin(3.5f * u.time + uv.y * u.screen.y * 1.5f));\n"
"    float s = powr(scanlines, 1.7f);\n"
"    color = color * float3(0.4f + 0.7f * s);\n"
"\n"
"    color *= 1.0f + 0.01f * sin(110.0f * u.time);\n"
"    if (uv.x < 0.0f || uv.x > 1.0f) {\n"
"        color *= 0.0;\n"
"    }\n"
"    if (uv.y < 0.0f || uv.y > 1.0f) {\n"
"        color *= 0.0;\n"
"    }\n"
"\n"
"    color *= 1.0f - 0.65f * float3(saturate((fmod(in.pos.x, 2.0f) - 1.0f) * 2.0f));\n"
"    return float4(color, 1.0f);\n"
"}\n";

typedef struct {
	simd_float2 screen;
	simd_float2 fade;
	float time;
} shader_uniforms_t;

// -----------------------------------------------------------------------------
// Rendering

typedef struct {
	vec2i_t offset;
	vec2i_t size;
} atlas_pos_t;

texture_t RENDER_NO_TEXTURE;

// This should be kept in sync with the number of blend modes.
#define RENDER_BLEND_COUNT (RENDER_BLEND_LIGHTER + 1)

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct {
	id<MTLDevice> device;
	id<MTLCommandQueue> commandQueue;
	id<MTLLibrary> library;
	id<MTLRenderPipelineState> mainRenderPipelines[RENDER_BLEND_COUNT];
	id<MTLRenderPipelineState> postRenderPipelines[RENDER_POST_MAX];
	id<MTLTexture> atlasTexture;
	id<MTLTexture> backbufferTexture;
	id<MTLSamplerState> sampler;
	id<MTLBuffer> vertexBuffer;
	id<MTLBuffer> indexBuffer;
	id<MTLCommandBuffer> currentCommandBuffer;
	dispatch_semaphore_t frameSemaphore;
} mtl_ctxt_t;

static mtl_ctxt_t mtl;

static const MTLPixelFormat atlasFormat = MTLPixelFormatRGBA8Unorm;
static const MTLPixelFormat renderbufferFormat = MTLPixelFormatBGRA8Unorm;

static uint32_t atlasMap[RENDER_ATLAS_SIZE];
static atlas_pos_t textures[RENDER_TEXTURES_MAX];
static uint32_t textureCount = 0;
static const BOOL useMipmaps = RENDER_USE_MIPMAPS ? YES : NO;
static BOOL regenerateMipmaps = NO;
static vec2i_t screenSize;
static vec2i_t backbufferSize;
static BOOL clearBackbuffer = NO;
static quadverts_t quadBuffer[RENDER_BUFFER_CAPACITY];
static uint32_t quadCount = 0;
static render_blend_mode_t blendMode = RENDER_BLEND_NORMAL;
static uint32_t postEffectIndex = 0;
static const uint32_t vertexBufferLength = sizeof(quadverts_t) * RENDER_BUFFER_CAPACITY * MAX_FRAMES_IN_FLIGHT;
static uint32_t vertexBufferOffset = 0;

void render_backend_init(void) {
	mtl.device = MTLCreateSystemDefaultDevice();
	mtl.commandQueue = [mtl.device newCommandQueue];
	mtl.currentCommandBuffer = nil;

	CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
	layer.device = mtl.device;
	layer.pixelFormat = renderbufferFormat;

	NSError *error = nil;
	mtl.library = [mtl.device newLibraryWithSource:shaderSource options:nil error:&error];
	error_if(error != nil, "Error occurred when creating library: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	MTLRenderPipelineDescriptor *pipelineDescriptor = [MTLRenderPipelineDescriptor new];
	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_main"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_main"];
	pipelineDescriptor.colorAttachments[0].pixelFormat = renderbufferFormat;
	pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;

	for (int blendMode = 0; blendMode < RENDER_BLEND_COUNT; ++blendMode) {
		switch (blendMode) {
			case RENDER_BLEND_NORMAL:
				pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
				pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
				break;
			case RENDER_BLEND_LIGHTER:
				pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
				pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
				pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
				pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
				break;
		}

		mtl.mainRenderPipelines[blendMode] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
		error_if(error != nil, "Error occurred when creating render pipeline: %s",
				 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);
	}

	pipelineDescriptor.colorAttachments[0].blendingEnabled = NO;

	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_post"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_post_default"];
	mtl.postRenderPipelines[RENDER_POST_NONE] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	error_if(error != nil, "Error occurred when creating render pipeline: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	pipelineDescriptor.vertexFunction = [mtl.library newFunctionWithName:@"vertex_post"];
	pipelineDescriptor.fragmentFunction = [mtl.library newFunctionWithName:@"fragment_post_crt"];
	mtl.postRenderPipelines[RENDER_POST_CRT] = [mtl.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
	error_if(error != nil, "Error occurred when creating render pipeline: %s",
			 [error.localizedDescription cStringUsingEncoding:NSUTF8StringEncoding]);

	uint32_t atlasWidth = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
	uint32_t atlasHeight = RENDER_ATLAS_SIZE * RENDER_ATLAS_GRID;
	MTLTextureDescriptor *atlasDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:atlasFormat
																							   width:atlasWidth
																							  height:atlasHeight
																						   mipmapped:useMipmaps];
	atlasDescriptor.usage = MTLTextureUsageShaderRead;
	mtl.atlasTexture = [mtl.device newTextureWithDescriptor:atlasDescriptor];

	MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
	samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
	samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
	samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
	samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
	samplerDescriptor.mipFilter = RENDER_USE_MIPMAPS ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
	mtl.sampler = [mtl.device newSamplerStateWithDescriptor:samplerDescriptor];

	mtl.vertexBuffer = [mtl.device newBufferWithLength:vertexBufferLength
											   options:MTLResourceStorageModeShared];

	uint16_t indices[RENDER_BUFFER_CAPACITY][6];
	for (uint32_t i = 0, j = 0; i < RENDER_BUFFER_CAPACITY; i++, j += 4) {
		indices[i][0] = j + 3;
		indices[i][1] = j + 1;
		indices[i][2] = j + 0;
		indices[i][3] = j + 3;
		indices[i][4] = j + 2;
		indices[i][5] = j + 1;
	}
	mtl.indexBuffer = [mtl.device newBufferWithBytes:&indices
											  length:sizeof(uint16_t) * 6 * RENDER_BUFFER_CAPACITY
											 options:MTLResourceStorageModeShared];

	rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
	RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);

	mtl.frameSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

void render_backend_cleanup(void) {
	mtl.indexBuffer = nil;
	mtl.vertexBuffer = nil;
	mtl.sampler = nil;
	mtl.backbufferTexture = nil;
	mtl.atlasTexture = nil;
	for (int i = 0; i < RENDER_POST_MAX; ++i) {
		mtl.postRenderPipelines[i] = nil;
	}
	for (int i = 0; i < RENDER_BLEND_COUNT; ++i) {
		mtl.mainRenderPipelines[i] = nil;
	}
	mtl.library = nil;
	mtl.commandQueue = nil;
	mtl.device = nil;
}

static uint32_t alignup(uint32_t n, uint32_t alignment) {
	return ((n + alignment - 1) / alignment) * alignment;
}

static void render_flush(id<MTLRenderPipelineState> renderPipeline,
						 id<MTLTexture> sourceTexture,
						 id<MTLTexture> destinationTexture)
{
	if (regenerateMipmaps) {
		id<MTLBlitCommandEncoder> mipmapEncoder = [mtl.currentCommandBuffer blitCommandEncoder];
		[mipmapEncoder generateMipmapsForTexture:mtl.atlasTexture];
		[mipmapEncoder endEncoding];
		regenerateMipmaps = NO;
	}

	MTLRenderPassDescriptor *passDescriptor = [MTLRenderPassDescriptor new];
	passDescriptor.colorAttachments[0].texture = destinationTexture;
	passDescriptor.colorAttachments[0].loadAction = clearBackbuffer ? MTLLoadActionClear : MTLLoadActionLoad;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
	passDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	clearBackbuffer = NO; // Accumulate multiple passes if needed due to quad buffer size

	id<MTLRenderCommandEncoder> renderCommandEncoder = [mtl.currentCommandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

	[renderCommandEncoder setFrontFacingWinding:MTLWindingClockwise];
	[renderCommandEncoder setCullMode:MTLCullModeBack];

	MTLViewport viewport = { 0, 0, backbufferSize.x, backbufferSize.y, 0, 1 };
	[renderCommandEncoder setViewport:viewport];

	[renderCommandEncoder setRenderPipelineState:renderPipeline];

	if (vertexBufferOffset + sizeof(quadverts_t) * quadCount >= vertexBufferLength) {
		vertexBufferOffset = 0;
	}
	memcpy([mtl.vertexBuffer contents] + vertexBufferOffset, quadBuffer, sizeof(quadverts_t) * quadCount);
	[renderCommandEncoder setVertexBuffer:mtl.vertexBuffer offset:vertexBufferOffset atIndex:0];
	vertexBufferOffset = alignup(vertexBufferOffset + sizeof(quadverts_t) * quadCount, 256);

	shader_uniforms_t uniforms = {
		simd_make_float2(screenSize.x, screenSize.y),
		simd_make_float2(0.0f, 0.0f),
		engine.time
	};
	[renderCommandEncoder setVertexBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];
	[renderCommandEncoder setFragmentBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];

	[renderCommandEncoder setFragmentTexture:sourceTexture atIndex:0];
	[renderCommandEncoder setFragmentSamplerState:mtl.sampler atIndex:0];

	if (quadCount != 0) {
		[renderCommandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
										 indexCount:quadCount * 6
										  indexType:MTLIndexTypeUInt16
										indexBuffer:mtl.indexBuffer
								  indexBufferOffset:0];
	}
	quadCount = 0;

	[renderCommandEncoder endEncoding];
}

void render_set_screen(vec2i_t size) {
	screenSize = size;
	backbufferSize = size;

	MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:renderbufferFormat
																						  width:backbufferSize.x
																						 height:backbufferSize.y
																					  mipmapped:NO];
	descriptor.storageMode = MTLStorageModePrivate;
	descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

	mtl.backbufferTexture = [mtl.device newTextureWithDescriptor:descriptor];
}

void render_set_blend_mode(render_blend_mode_t mode) {
	if (mode == blendMode) {
		return;
	}
	render_flush(mtl.mainRenderPipelines[blendMode], mtl.atlasTexture, mtl.backbufferTexture);

	blendMode = mode;
}

void render_set_post_effect(render_post_effect_t post) {
	error_if(post < 0 || post > RENDER_POST_MAX, "Invalid post effect %d", post);
	postEffectIndex = post;
}

void render_frame_prepare(void) {
	@autoreleasepool {
		dispatch_semaphore_wait(mtl.frameSemaphore, DISPATCH_TIME_NOW);
		mtl.currentCommandBuffer = [mtl.commandQueue commandBuffer];
		clearBackbuffer = YES;
	}
}

void render_frame_end(void) {
	@autoreleasepool {
		// Main Pass
		render_flush(mtl.mainRenderPipelines[blendMode], mtl.atlasTexture, mtl.backbufferTexture);

		// Post Pass and Present
		CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
		id<CAMetalDrawable> drawable = [layer nextDrawable];
		if (drawable) {
			quadBuffer[quadCount] = (quadverts_t){
				.vertices = {
					{.pos = {0,            0           }, .uv = {0, 0}, .color = rgba_white()},
					{.pos = {screenSize.x, 0           }, .uv = {1, 0}, .color = rgba_white()},
					{.pos = {screenSize.x, screenSize.y}, .uv = {1, 1}, .color = rgba_white()},
					{.pos = {0,            screenSize.y}, .uv = {0, 1}, .color = rgba_white()},
				}
			};
			++quadCount;

			clearBackbuffer = YES;
			render_flush(mtl.postRenderPipelines[postEffectIndex], mtl.backbufferTexture, drawable.texture);

			[mtl.currentCommandBuffer presentDrawable:drawable];
		}

		[mtl.currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
			dispatch_semaphore_signal(mtl.frameSemaphore);
		}];

		[mtl.currentCommandBuffer commit];
		mtl.currentCommandBuffer = nil;
	}
}

void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);
	atlas_pos_t *t = &textures[texture_handle.index];

	if (quadCount >= RENDER_BUFFER_CAPACITY) {
		render_flush(mtl.mainRenderPipelines[blendMode], mtl.atlasTexture, mtl.backbufferTexture);
	}

	quadBuffer[quadCount] = *quad;
	for (uint32_t i = 0; i < 4; i++) {
		quadBuffer[quadCount].vertices[i].uv.x =
		(quadBuffer[quadCount].vertices[i].uv.x + t->offset.x) * (1.0 / RENDER_ATLAS_SIZE_PX);
		quadBuffer[quadCount].vertices[i].uv.y =
		(quadBuffer[quadCount].vertices[i].uv.y + t->offset.y) * (1.0 / RENDER_ATLAS_SIZE_PX);
	}
	++quadCount;
}

// -----------------------------------------------------------------------------
// Textures

texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textureCount };
}

void textures_reset(texture_mark_t mark) {
	error_if(mark.index > textureCount, "Invalid texture reset mark %d >= %d", mark.index, textureCount);
	if (mark.index == textureCount) {
		return;
	}

	render_flush(mtl.mainRenderPipelines[blendMode], mtl.atlasTexture, mtl.backbufferTexture);

	textureCount = mark.index;
	clear(atlasMap);

	// Clear completely and recreate the default white texture
	if (textureCount == 0) {
		rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
		RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);
		return;
	}

	// Replay all texture grid insertions up to the reset len
	for (int i = 0; i < textureCount; i++) {
		uint32_t grid_x = (textures[i].offset.x - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_y = (textures[i].offset.y - RENDER_ATLAS_BORDER) / RENDER_ATLAS_GRID;
		uint32_t grid_width = (textures[i].size.x + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		uint32_t grid_height = (textures[i].size.y + RENDER_ATLAS_BORDER * 2 + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
		for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
			atlasMap[cx] = grid_y + grid_height;
		}
	}
}

texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	error_if(textureCount >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");

	uint32_t bw = size.x + RENDER_ATLAS_BORDER * 2;
	uint32_t bh = size.y + RENDER_ATLAS_BORDER * 2;

	// Find a position in the atlas for this texture (with added border)
	uint32_t grid_width = (bw + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
	uint32_t grid_height = (bh + RENDER_ATLAS_GRID - 1) / RENDER_ATLAS_GRID;
	uint32_t grid_x = 0;
	uint32_t grid_y = RENDER_ATLAS_SIZE - grid_height + 1;

	error_if(grid_width > RENDER_ATLAS_SIZE || grid_height > RENDER_ATLAS_SIZE, "Texture of size %dx%d doesn't fit in atlas", size.x, size.y);

	for (uint32_t cx = 0; cx < RENDER_ATLAS_SIZE - grid_width; cx++) {
		if (atlasMap[cx] >= grid_y) {
			continue;
		}

		uint32_t cy = atlasMap[cx];
		bool is_best = true;

		for (uint32_t bx = cx; bx < cx + grid_width; bx++) {
			if (atlasMap[bx] >= grid_y) {
				is_best = false;
				cx = bx;
				break;
			}
			if (atlasMap[bx] > cy) {
				cy = atlasMap[bx];
			}
		}
		if (is_best) {
			grid_y = cy;
			grid_x = cx;
		}
	}

	error_if(grid_y + grid_height > RENDER_ATLAS_SIZE, "Render atlas ran out of space for %dx%d texture", size.x, size.y);

	for (uint32_t cx = grid_x; cx < grid_x + grid_width; cx++) {
		atlasMap[cx] = grid_y + grid_height;
	}

	uint32_t x = grid_x * RENDER_ATLAS_GRID;
	uint32_t y = grid_y * RENDER_ATLAS_GRID;

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

	[mtl.atlasTexture replaceRegion:MTLRegionMake2D(x, y, bw, bh)
						mipmapLevel:0
						  withBytes:pixels
						bytesPerRow:bw * 4];
	temp_free(pb);
#else
	[mtl.atlasTexture replaceRegion:MTLRegionMake2D(x, y, bw, bh)
						mipmapLevel:0
						  withBytes:pixels
						bytesPerRow:bw * 4];
#endif

	regenerateMipmaps = useMipmaps;
	texture_t texture_handle = {.index = textureCount};
	textureCount++;
	textures[texture_handle.index] = (atlas_pos_t){.offset = {x + RENDER_ATLAS_BORDER, y + RENDER_ATLAS_BORDER}, .size = size};

	return texture_handle;
}

void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);

	atlas_pos_t *t = &textures[texture_handle.index];
	error_if(t->size.x < size.x || t->size.y < size.y, "Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, t->size.x, t->size.y);

	[mtl.atlasTexture replaceRegion:MTLRegionMake2D(t->offset.x, t->offset.y, size.x, size.y)
						mipmapLevel:0
						  withBytes:pixels
						bytesPerRow:size.x * 4];
	regenerateMipmaps = useMipmaps;
}
