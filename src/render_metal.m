#include "engine.h"
#include "render.h"
#include "alloc.h"
#include "utils.h"
#include "platform.h"

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

// Since we might flush mid-frame (due to a blend state change or texture update,
// we need to ensure that our instances are aligned to the minimum constant buffer
// offset alignment of the device, so that we can bind our vertex buffer at the
// granularity of a single instance. On MTLGPUFamilyMac2, this minimum alignment
// is 32 bytes.
#define RENDER_INSTANCE_ALIGNMENT 32

static char *const shaderSource = ""
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"#if !defined(RENDER_TEXTURES_MAX)\n"
"	#define RENDER_TEXTURES_MAX 1024\n"
"#endif\n"
"\n"
"#if !defined(RENDER_INSTANCE_ALIGNMENT)\n"
"	#define RENDER_INSTANCE_ALIGNMENT 32\n"
"#endif\n"
"\n"
"struct __attribute__((aligned(RENDER_INSTANCE_ALIGNMENT))) QuadInstance {\n"
"	float2 positions[4];\n"
"	float2 uvs[4];\n"
"	uint colors[4];\n"
"	uint textureIndex;\n"
"};\n"
"\n"
"struct VertexOut {\n"
"	float4 pos [[position]];\n"
"	float2 uv;\n"
"	float4 color;\n"
"	uint textureIndex [[flat]];\n"
"};\n"
"\n"
"struct Uniforms {\n"
"	float2 screen;\n"
"	float2 fade;\n"
"	float time;\n"
"};\n"
"\n"
"struct Arguments {\n"
"	texture2d<float> textures[RENDER_TEXTURES_MAX];\n"
"	sampler textureSampler;\n"
"};\n"
"\n"
"vertex VertexOut vertex_main(device const QuadInstance *instances [[buffer(0)]],\n"
"							 constant Uniforms &u                  [[buffer(1)]],\n"
"							 uint instanceID                       [[instance_id]],\n"
"							 uint vertexID                         [[vertex_id]])\n"
"{\n"
"	device const QuadInstance &quad = instances[instanceID];\n"
"	float2 pos = quad.positions[vertexID];\n"
"	float2 uv = quad.uvs[vertexID];\n"
"	uint color = quad.colors[vertexID];\n"
"\n"
"	VertexOut out {\n"
"		.pos = float4(floor(pos + 0.5f) * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f),\n"
"		.uv = uv,\n"
"		.color = unpack_unorm4x8_to_float(color),\n"
"		.textureIndex = quad.textureIndex,\n"
"	};\n"
"	out.pos.y *= -1.0f;\n"
"	return out;\n"
"}\n"
"\n"
"fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
"							  constant Arguments &arguments [[buffer(0)]])\n"
"{\n"
"	float4 tex_color = arguments.textures[in.textureIndex].sample(arguments.textureSampler, in.uv);\n"
"	float4 color = tex_color * in.color;\n"
"	return color;\n"
"}\n"
"\n"
"vertex VertexOut vertex_post(device const QuadInstance *instances [[buffer(0)]],\n"
"							  constant Uniforms &u           [[buffer(1)]],\n"
"							  uint instanceID                [[instance_id]],\n"
"							  uint vertexID                  [[vertex_id]])\n"
"{\n"
"	device const QuadInstance &quad = instances[instanceID];\n"
"	float2 pos = quad.positions[vertexID];\n"
"	float2 uv = quad.uvs[vertexID];\n"
"\n"
"	VertexOut out {\n"
"		.pos = float4(pos * (float2(2.0f, -2.0f) / u.screen.xy) + float2(-1.0f, 1.0f), 0.0f, 1.0f),\n"
"		.uv = uv,\n"
"		.color = float4(1.0f),\n"
"		.textureIndex = 0\n"
"	};\n"
"	out.pos.y *= -1.0f;\n"
"	return out;\n"
"}\n"
"\n"
"fragment float4 fragment_post_default(VertexOut in [[stage_in]],\n"
"									  constant Arguments &arguments [[buffer(0)]])\n"
"{\n"
"	return arguments.textures[in.textureIndex].sample(arguments.textureSampler, in.uv);\n"
"}\n"
"\n"
"// CRT effect based on https://www.shadertoy.com/view/Ms23DR\n"
"// by https://github.com/mattiasgustavsson/\n"
"\n"
"static float2 curve(float2 uv) {\n"
"	uv = (uv - 0.5f) * 2.0f;\n"
"	uv *= 1.1f;\n"
"	uv.x *= 1.0f + powr((abs(uv.y) / 5.0f), 2.0f);\n"
"	uv.y *= 1.0f + powr((abs(uv.x) / 4.0f), 2.0f);\n"
"	uv  = (uv / 2.0f) + 0.5f;\n"
"	uv =  uv * 0.92f + 0.04f;\n"
"	return uv;\n"
"}\n"
"\n"
"fragment float4 fragment_post_crt(VertexOut in                  [[stage_in]],\n"
"								  constant Arguments &arguments [[buffer(0)]],\n"
"								  constant Uniforms &u          [[buffer(1)]])\n"
"{\n"
"	auto screenbuffer = arguments.textures[in.textureIndex];\n"
"	auto screenSampler = arguments.textureSampler;\n"
"\n"
"	float2 uv = curve(in.uv);\n"
"	float3 color;\n"
"	float x = sin(0.3f * u.time + in.uv.y * 21.0f) * sin(0.7f * u.time + uv.y * 29.0f) *\n"
"	sin(0.3f + 0.33f * u.time + uv.y * 31.0f) * 0.0017f;\n"
"\n"
"	color.r = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.001f, uv.y + 0.001f)).x + 0.05f;\n"
"	color.g = screenbuffer.sample(screenSampler, float2(x + uv.x + 0.000f, uv.y - 0.002f)).y + 0.05f;\n"
"	color.b = screenbuffer.sample(screenSampler, float2(x + uv.x - 0.002f, uv.y + 0.000f)).z + 0.05f;\n"
"	color.r += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + 0.025f, -0.027f) + float2(uv.x + 0.001f, uv.y + 0.001f)).x;\n"
"	color.g += 0.05 * screenbuffer.sample(screenSampler, 0.75f * float2(x - 0.022f, -0.020f) + float2(uv.x + 0.000f, uv.y - 0.002f)).y;\n"
"	color.b += 0.08 * screenbuffer.sample(screenSampler, 0.75f * float2(x + -0.02f, -0.018f) + float2(uv.x - 0.002f, uv.y + 0.000f)).z;\n"
"\n"
"	color = saturate(color * 0.6f + 0.4f * color * color * 1.0f);\n"
"\n"
"	float vignette = (0.0f + 1.0f * 16.0f * uv.x * uv.y * (1.0f - uv.x) * (1.0f - uv.y));\n"
"	color *= float3(powr(vignette, 0.25f));\n"
"	color *= float3(0.95f, 1.05f, 0.95f);\n"
"	color *= 2.8;\n"
"\n"
"	float scanlines = saturate(0.35f + 0.35f * sin(3.5f * u.time + uv.y * u.screen.y * 1.5f));\n"
"	float s = powr(scanlines, 1.7f);\n"
"	color = color * float3(0.4f + 0.7f * s);\n"
"\n"
"	color *= 1.0f + 0.01f * sin(110.0f * u.time);\n"
"	if (uv.x < 0.0f || uv.x > 1.0f) {\n"
"		color *= 0.0;\n"
"	}\n"
"	if (uv.y < 0.0f || uv.y > 1.0f) {\n"
"		color *= 0.0;\n"
"	}\n"
"\n"
"	color *= 1.0f - 0.65f * float3(saturate((fmod(in.pos.x, 2.0f) - 1.0f) * 2.0f));\n"
"	return float4(color, 1.0f);\n"
"}\n";

typedef struct {
	simd_float2 screen;
	simd_float2 fade;
	float time;
} shader_uniforms_t;

typedef struct {
	MTLResourceID textures[RENDER_TEXTURES_MAX];
	MTLResourceID sampler;
} shader_arguments_t;

typedef struct __attribute__((aligned(RENDER_INSTANCE_ALIGNMENT))) {
	simd_float2 positions[4];
	simd_float2 uvs[4];
	uint32_t colors[4];
	uint32_t textureIndex;
} quad_instance_t;

// -----------------------------------------------------------------------------
// Rendering

texture_t RENDER_NO_TEXTURE;
static texture_t RENDER_BACKBUFFER_TEXTURE;

// This should be kept in sync with the number of blend modes.
#define RENDER_BLEND_COUNT (RENDER_BLEND_LIGHTER + 1)

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct {
	id<MTLDevice> device;
	id<MTLCommandQueue> commandQueue;
	id<MTLLibrary> library;
	id<MTLRenderPipelineState> mainRenderPipelines[RENDER_BLEND_COUNT];
	id<MTLRenderPipelineState> postRenderPipelines[RENDER_POST_MAX];
	id<MTLTexture> textures[RENDER_TEXTURES_MAX];
	id<MTLSamplerState> sampler;
	id<MTLBuffer> instanceBuffers[MAX_FRAMES_IN_FLIGHT];
	id<MTLBuffer> argumentBuffer;
	id<MTLCommandBuffer> currentCommandBuffer;
	dispatch_semaphore_t frameSemaphore;
} mtl_ctxt_t;

static mtl_ctxt_t mtl;

static const MTLPixelFormat atlasFormat = MTLPixelFormatRGBA8Unorm;
static const MTLPixelFormat renderbufferFormat = MTLPixelFormatBGRA8Unorm;

static const BOOL useMipmaps = RENDER_USE_MIPMAPS ? YES : NO;
static vec2i_t screenSize;
static vec2i_t backbufferSize;
static BOOL reencodeArgumentBuffer = YES;
static BOOL clearBackbuffer = NO;
static size_t textureCount;
static size_t frameIndex;
static size_t maxInstanceCount;
static size_t instanceCount;
static size_t instanceBufferLength;
static size_t instanceBufferReadOffset;
static size_t instanceBufferWriteOffset;
static render_blend_mode_t blendMode = RENDER_BLEND_NORMAL;
static size_t postEffectIndex = 0;

static uint32_t alignup(uint32_t n, uint32_t alignment) {
	return ((n + alignment - 1) / alignment) * alignment;
}

static void render_realloc_instance_storage(size_t newMaxInstanceCount) {
	size_t alignedInstanceSize = alignup(sizeof(quad_instance_t), RENDER_INSTANCE_ALIGNMENT);
	// When performing instancing, instance data must be packed (allowing for padding
	// inside instance structs but not between them). Hence the aligned size actually
	// has to be the size of the struct.
	error_if(sizeof(quad_instance_t) != alignedInstanceSize,
			 "Instance size must be exactly equal to aligned instance size");
	if (newMaxInstanceCount > maxInstanceCount) {
		size_t bufferLength = alignedInstanceSize * newMaxInstanceCount;
		size_t currentBufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
		id<MTLBuffer> inProgressBuffer = mtl.instanceBuffers[currentBufferIndex];
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			mtl.instanceBuffers[i] = [mtl.device newBufferWithLength:bufferLength
															 options:MTLResourceStorageModeShared];
		}
		if (inProgressBuffer) {
			// If we're mid-frame, we need to preserve buffer contents because there might be instances
			// that have been copied but not yet encoded, within the range [instanceBufferReadOffset,
			// instanceBufferWriteOffset).
			memcpy([mtl.instanceBuffers[currentBufferIndex] contents],
				   [inProgressBuffer contents],
				   inProgressBuffer.length);
		}
		instanceBufferLength = bufferLength;
		maxInstanceCount = newMaxInstanceCount;
	}
}

static quad_instance_t *render_alloc_quads(size_t count) {
	size_t alignedInstanceSize = alignup(sizeof(quad_instance_t), RENDER_INSTANCE_ALIGNMENT);
	while (instanceBufferWriteOffset + alignedInstanceSize * count >= instanceBufferLength) {
		render_realloc_instance_storage(maxInstanceCount * 2);
	}
	size_t bufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	id<MTLBuffer> currentInstanceBuffer = mtl.instanceBuffers[bufferIndex];
	size_t instanceOffset = instanceBufferWriteOffset;
	instanceBufferWriteOffset += alignedInstanceSize * count;
	return (quad_instance_t *)([currentInstanceBuffer contents] + instanceOffset);
}

void render_backend_init(void) {
	mtl.device = MTLCreateSystemDefaultDevice();
	mtl.commandQueue = [mtl.device newCommandQueue];
	mtl.currentCommandBuffer = nil;

	BOOL supportsBindless = mtl.device.argumentBuffersSupport == MTLArgumentBuffersTier2;
	error_if(!supportsBindless, "Metal renderer requires support for argument buffers tier 2");

	CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
	layer.device = mtl.device;
	layer.pixelFormat = renderbufferFormat;

	MTLCompileOptions *options = [MTLCompileOptions new];
	options.preprocessorMacros = @{
		@"RENDER_TEXTURES_MAX" : @(RENDER_TEXTURES_MAX),
		@"RENDER_INSTANCE_ALIGNMENT" : @(RENDER_INSTANCE_ALIGNMENT)
	};
	NSError *error = nil;
	mtl.library = [mtl.device newLibraryWithSource:[NSString stringWithUTF8String:shaderSource]
										   options:options
											 error:&error];
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

	// Reserve texture slot for backbuffer
	RENDER_BACKBUFFER_TEXTURE = (texture_t){ .index = textureCount++ };

	MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
	samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
	samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
	samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
	samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
	samplerDescriptor.mipFilter = RENDER_USE_MIPMAPS ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
	samplerDescriptor.supportArgumentBuffers = YES;
	mtl.sampler = [mtl.device newSamplerStateWithDescriptor:samplerDescriptor];

	render_realloc_instance_storage(RENDER_BUFFER_CAPACITY);

	mtl.argumentBuffer = [mtl.device newBufferWithLength:sizeof(shader_arguments_t)
												 options:MTLResourceStorageModeShared];

	rgba_t white_pixels[4] = {rgba_white(), rgba_white(), rgba_white(), rgba_white()};
	RENDER_NO_TEXTURE = texture_create(vec2i(2, 2), white_pixels);

	mtl.frameSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

void render_backend_cleanup(void) {
	mtl.sampler = nil;
	for (int i = 0; i < textureCount; ++i) {
		mtl.textures[i] = nil;
	}
	textureCount = 0;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		mtl.instanceBuffers[i] = nil;
	}
	mtl.argumentBuffer = nil;
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

static void render_flush(id<MTLRenderPipelineState> renderPipeline, id<MTLTexture> destinationTexture)
{
	if (reencodeArgumentBuffer) {
		shader_arguments_t *args = (shader_arguments_t *)[mtl.argumentBuffer contents];
		for (int i = 0; i < textureCount; ++i) {
			args->textures[i] = mtl.textures[i].gpuResourceID;
		}
		args->sampler = mtl.sampler.gpuResourceID;
		reencodeArgumentBuffer = NO;
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
	[renderCommandEncoder useResources:mtl.textures count:textureCount usage:MTLResourceUsageRead];

	shader_uniforms_t uniforms = {
		simd_make_float2(screenSize.x, screenSize.y),
		simd_make_float2(0.0f, 0.0f),
		engine.time
	};
	size_t bufferIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	[renderCommandEncoder setVertexBuffer:mtl.instanceBuffers[bufferIndex] offset:instanceBufferReadOffset atIndex:0];
	[renderCommandEncoder setVertexBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];
	[renderCommandEncoder setFragmentBuffer:mtl.argumentBuffer offset:0 atIndex:0];
	[renderCommandEncoder setFragmentBytes:&uniforms length:sizeof(shader_uniforms_t) atIndex:1];

	if (instanceCount != 0) {
		[renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
								 vertexStart:0
								 vertexCount:4
							   instanceCount:instanceCount
								baseInstance:0];
		instanceBufferReadOffset = instanceBufferWriteOffset;
		instanceCount = 0;
	}

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
	id<MTLTexture> backbufferTexture = [mtl.device newTextureWithDescriptor:descriptor];
	mtl.textures[RENDER_BACKBUFFER_TEXTURE.index] = backbufferTexture;
	reencodeArgumentBuffer = YES;
}

void render_set_blend_mode(render_blend_mode_t mode) {
	if (mode == blendMode) {
		return;
	}
	render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);
	blendMode = mode;
}

void render_set_post_effect(render_post_effect_t post) {
	error_if(post < 0 || post > RENDER_POST_MAX, "Invalid post effect %d", post);
	postEffectIndex = post;
}

void render_frame_prepare(void) {
	@autoreleasepool {
		dispatch_semaphore_wait(mtl.frameSemaphore, 1 * NSEC_PER_SEC);
		mtl.currentCommandBuffer = [mtl.commandQueue commandBuffer];
		clearBackbuffer = YES;
		instanceBufferReadOffset = 0;
		instanceBufferWriteOffset = 0;
	}
}

void render_frame_end(void) {
	@autoreleasepool {
		// Main Pass
		render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);

		// Post Pass and Present
		CAMetalLayer *layer = (__bridge CAMetalLayer *)platform_get_metal_layer();
		id<CAMetalDrawable> drawable = [layer nextDrawable];
		if (drawable) {
			quad_instance_t *instance = render_alloc_quads(1);
			quad_instance_t quad = {
				.positions = {{0, 0}, {0, screenSize.y}, {screenSize.x, 0}, {screenSize.x, screenSize.y}},
				.uvs = {{0, 0},{0, 1}, {1, 0}, {1, 1}},
				.colors = {rgba_white().v, rgba_white().v, rgba_white().v, rgba_white().v},
				.textureIndex = RENDER_BACKBUFFER_TEXTURE.index
			};
			memcpy(instance, &quad, sizeof(quad_instance_t));
			++instanceCount;

			clearBackbuffer = YES;
			render_flush(mtl.postRenderPipelines[postEffectIndex], drawable.texture);

			[mtl.currentCommandBuffer presentDrawable:drawable];
		}

		[mtl.currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
			dispatch_semaphore_signal(mtl.frameSemaphore);
		}];

		[mtl.currentCommandBuffer commit];
		mtl.currentCommandBuffer = nil;
	}
	++frameIndex;
}

void render_draw_quad(quadverts_t *quad, texture_t texture_handle) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);
	id<MTLTexture> texture = mtl.textures[texture_handle.index];
	quad_instance_t *instance = render_alloc_quads(1);
	int reorder[] = { 1, 0, 2, 3 }; // Swaps vertices so they can be drawn as triangle strips
	for (uint32_t i = 0; i < 4; i++) {
		instance->positions[reorder[i]] = (simd_float2){quad->vertices[i].pos.x, quad->vertices[i].pos.y};
		instance->uvs[reorder[i]].x = quad->vertices[i].uv.x / texture.width;
		instance->uvs[reorder[i]].y = quad->vertices[i].uv.y / texture.height;
		instance->colors[reorder[i]] = quad->vertices[i].color.v;
	}
	instance->textureIndex = texture_handle.index;
	++instanceCount;
}

// -----------------------------------------------------------------------------
// Textures

texture_mark_t textures_mark(void) {
	return (texture_mark_t){.index = textureCount };
}

void textures_reset(texture_mark_t mark) {
	error_if(mark.index > textureCount, "Invalid texture reset mark %d >= %d", mark.index, textureCount);
	error_if(mark.index < 2, "Invalid texture reset mark %d < %d", mark.index, 2);
	if (mark.index == textureCount) {
		return;
	}
	render_flush(mtl.mainRenderPipelines[blendMode], mtl.textures[RENDER_BACKBUFFER_TEXTURE.index]);
	for (int i = mark.index; i < textureCount; ++i) {
		mtl.textures[i] = nil;
	}
	textureCount = mark.index;

	reencodeArgumentBuffer = YES;
}

static void texture_generate_mipmaps(id<MTLTexture> texture) {
	if (useMipmaps) {
		id<MTLCommandBuffer> commandBuffer = [mtl.commandQueue commandBuffer];
		id<MTLBlitCommandEncoder> mipmapEncoder = [commandBuffer blitCommandEncoder];
		[mipmapEncoder generateMipmapsForTexture:texture];
		[mipmapEncoder endEncoding];
		[commandBuffer commit];
	}
}

texture_t texture_create(vec2i_t size, rgba_t *pixels) {
	error_if(textureCount >= RENDER_TEXTURES_MAX, "RENDER_TEXTURES_MAX reached");
	MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:atlasFormat
																						  width:size.x
																						 height:size.y
																					  mipmapped:useMipmaps];
	descriptor.usage = MTLTextureUsageShaderRead;
	if (mtl.device.hasUnifiedMemory) {
		descriptor.storageMode = MTLResourceStorageModeShared;
	}
	id<MTLTexture> texture = [mtl.device newTextureWithDescriptor:descriptor];
	[texture replaceRegion:MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel:0 withBytes:pixels bytesPerRow:size.x * 4];
	texture_t texture_handle = {.index = textureCount};
	mtl.textures[textureCount] = texture;
	++textureCount;
	texture_generate_mipmaps(texture);
	reencodeArgumentBuffer = YES;
	return texture_handle;
}

void texture_replace_pixels(texture_t texture_handle, vec2i_t size, rgba_t *pixels) {
	error_if(texture_handle.index >= textureCount, "Invalid texture %d", texture_handle.index);
	id<MTLTexture> texture = mtl.textures[texture_handle.index];
	error_if(texture.width < size.x || texture.height < size.y,
			 "Cannot replace %dx%d pixels of %dx%d texture", size.x, size.y, texture.width, texture.height);
	[texture replaceRegion:MTLRegionMake2D(0, 0, size.x, size.y) mipmapLevel:0 withBytes:pixels bytesPerRow:size.x * 4];
	texture_generate_mipmaps(texture);
}
