#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>


#include "render.h"


typedef struct metal_shader
{
	id<MTLRenderPipelineState> PipelineState;
	id<MTLLibrary> Library;
} metal_shader;

#define FRAMES_IN_FLIGHT (3)

typedef struct metal_context
{
	id<MTLCommandQueue>        CommandQueue;
	id<MTLDevice>              Device;
	
	metal_shader               BloomShader;
	metal_shader               TriangleShader;
	metal_shader               GlyphShader;

	id<MTLTexture>             DepthTexture;
	id<MTLTexture>             BaseFrameBuffer;
	id<MTLTexture>             BlurredFrameBuffer;

	id<MTLTexture>             FontAtlasTexture;

	id<MTLBuffer>              TriangleVertexBuffers[FRAMES_IN_FLIGHT];
	id<MTLBuffer>              GlyphVertexBuffers[FRAMES_IN_FLIGHT];
	id<MTLBuffer>              IndexBuffer;
	id<MTLBuffer>              UniformBuffers[FRAMES_IN_FLIGHT];
	id<MTLSamplerState>        SamplerState;
	id<MTLSamplerState>        BloomSamplerState;

	id<CAMetalDrawable>        Drawable;
}
metal_context;