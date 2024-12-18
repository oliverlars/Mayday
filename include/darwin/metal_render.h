#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>


#include "render.h"

typedef struct metal_context
{
	id<MTLCommandQueue>        CommandQueue;
	id<MTLDevice>              Device;
	id<MTLRenderPipelineState> TrianglePipelineState;
	id<MTLRenderPipelineState> BloomPipelineState;
	id<MTLDepthStencilState>   DepthStencilState;
	id<MTLTexture>             DepthTexture;
	id<MTLTexture>             BaseFrameBuffer;
	id<MTLTexture>             BlurredFrameBuffer;

	id<MTLTexture>             FontAtlasTexture;
	id<MTLBuffer>              VertexBuffer;
	id<MTLBuffer>              IndexBuffer;
	id<MTLBuffer>              UniformBuffer;
	id<MTLSamplerState>        SamplerState;
	id<MTLSamplerState>        BloomSamplerState;
}
metal_context;