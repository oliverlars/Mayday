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

typedef struct metal_compute_shader
{
	id<MTLComputePipelineState> PipelineState;
	id<MTLLibrary> Library;
} metal_compute_shader;

#define FRAMES_IN_FLIGHT (3)

typedef struct metal_context
{
	id<MTLCommandQueue>        CommandQueue;
	id<MTLDevice>              Device;
	
	metal_shader               BloomShader;
	metal_shader               TriangleShader;
	metal_shader               GlyphShader;
	metal_shader               PointShader;
	metal_shader               LineShader;
	metal_shader               BezierShader;


	u8                         *SDFs;
	id<MTLBuffer>              Data[FRAMES_IN_FLIGHT];
	id<MTLBuffer>              PerTileData[FRAMES_IN_FLIGHT];
	metal_compute_shader       ComputeTest;

	id<MTLTexture>             DepthTexture;
	id<MTLTexture>             BaseFrameBuffer;
	id<MTLTexture>             TempFrameBuffer;
	id<MTLTexture>             BlurredFrameBuffer;

	id<MTLBuffer>              CurveBuffer[FRAMES_IN_FLIGHT];

	v2f                        *Curves;
	int                        TotalCurvesSoFar;

	id<MTLTexture>             FontAtlasTexture;

	id<MTLBuffer>              VertexBuffers[FRAMES_IN_FLIGHT];
	id<MTLBuffer>              UniformBuffers[FRAMES_IN_FLIGHT];
	id<MTLSamplerState>        SamplerState;
	id<MTLSamplerState>        BloomSamplerState;

	id<CAMetalDrawable>        Drawable;
	dispatch_semaphore_t       InFlightSemaphore;
}
metal_context;