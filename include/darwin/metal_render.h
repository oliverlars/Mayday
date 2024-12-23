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

typedef struct metal_context
{
	id<MTLCommandQueue>        CommandQueue;
	id<MTLDevice>              Device;
	
	metal_shader               BloomShader;
	metal_shader               TriangleShader;
	metal_shader               GlyphShader;

	id<MTLDepthStencilState>   DepthStencilState;
	id<MTLTexture>             DepthTexture;
	id<MTLTexture>             BaseFrameBuffer;
	id<MTLTexture>             BlurredFrameBuffer;

	id<MTLTexture>             FontAtlasTexture;

	id<MTLBuffer>              TriangleVertexBuffer;
	id<MTLBuffer>              GlyphVertexBuffer;
	id<MTLBuffer>              IndexBuffer;
	id<MTLBuffer>              UniformBuffer;
	id<MTLSamplerState>        SamplerState;
	id<MTLSamplerState>        BloomSamplerState;

	id<CAMetalDrawable>        Drawable;
}
metal_context;