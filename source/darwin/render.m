#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>

#include "sokol_app.h"

#include "os.h"
#include "maths.h"
#include "render.h"
#include "shaders.h"
#include "log.h"

id<MTLCommandQueue>        CommandQueue;
id<MTLDevice>              Device;
id<MTLRenderPipelineState> PipelineState;
id<MTLDepthStencilState>   DepthStencilState;
id<MTLTexture>             DepthTexture;
id<MTLBuffer>              VertexBuffer;
id<MTLBuffer>              IndexBuffer;
id<MTLBuffer>              UniformBuffer;

typedef struct vertex
{
	v4f Position;
	v4f Colour;
} vertex;

typedef struct uniforms
{
	m44f PerspectiveMatrix;
	m44f ViewMatrix;
	m44f ModelMatrix;
} uniforms;

vertex Vertices[8];
s32 Indices[36];

uniforms Uniforms;


id<MTLTexture> MakeDepthTexture(r_context *RenderContext, CGSize Size)
{
	MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float 
	                                                   width:Size.width
	                                                   height:Size.height mipmapped:NO];
	Desc.usage = MTLTextureUsageRenderTarget;
	Desc.storageMode = MTLStorageModeMemoryless;

	id<MTLTexture> Result = [Device newTextureWithDescriptor:Desc];
	assert(Result != nil);
	return Result;
}

id<MTLTexture> ResizeDepthTexture(r_context *RenderContext, CGSize Size)
{
	if(Size.width != [DepthTexture width] || Size.height != [DepthTexture height] || DepthTexture == nil)
	{
		DepthTexture = nil;

		DepthTexture = MakeDepthTexture(RenderContext, Size);

		LogInfo(&RenderContext->Logger, "DepthTexture resized");
	}
	return DepthTexture;
}

void R_Init(r_context *RenderContext)
{
	Vertices[0] =  (vertex){{ 1,  1,  1, 1}, {1.0, 0.0, 0.0, 1.0}};
	Vertices[1] =  (vertex){{ 1, -1,  1, 1}, {1.0, 0.0, 0.0, 1.0}};
	Vertices[2] =  (vertex){{-1, -1,  1, 1}, {1.0, 0.0, 0.0, 1.0}};
	Vertices[3] =  (vertex){{-1,  1,  1, 1}, {1.0, 0.0, 0.0, 1.0}};

	Vertices[4] =  (vertex){{ 1,  1, -1, 1}, {0.0, 1.0, 0.0, 1.0}};
	Vertices[5] =  (vertex){{ 1, -1, -1, 1}, {0.0, 1.0, 0.0, 1.0}};
	Vertices[6] =  (vertex){{-1, -1, -1, 1}, {0.0, 1.0, 0.0, 1.0}};
	Vertices[7] =  (vertex){{-1,  1, -1, 1}, {0.0, 1.0, 0.0, 1.0}};

	Indices[0] = 0;
	Indices[1] = 1;
	Indices[2] = 2;

	Indices[3] = 0;
	Indices[4] = 2;
	Indices[5] = 3;

	Indices[6] = 4;
	Indices[7] = 6;
	Indices[8] = 5;

	Indices[9] = 4;
	Indices[10] = 7;
	Indices[11] = 6;

	Indices[12] = 0;
	Indices[13] = 4;
	Indices[14] = 5;

	Indices[15] = 0;
	Indices[16] = 5;
	Indices[17] = 1;

	Indices[18] = 2;
	Indices[19] = 6;
	Indices[20] = 7;

	Indices[21] = 2;
	Indices[22] = 7;
	Indices[23] = 3;

	Indices[24] = 0;
	Indices[25] = 3;
	Indices[26] = 7;

	Indices[27] = 0;
	Indices[28] = 7;
	Indices[29] = 4;

	Indices[30] = 1;
	Indices[31] = 5;
	Indices[32] = 6;

	Indices[33] = 1;
	Indices[34] = 6;
	Indices[35] = 2;

	Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	CommandQueue = [Device newCommandQueue];

	VertexBuffer = [Device newBufferWithBytes:Vertices
					length:sizeof(Vertices)
					options:MTLResourceStorageModePrivate];

	UniformBuffer = [Device newBufferWithLength:sizeof(Uniforms)
					 options:MTLResourceStorageModePrivate];

	IndexBuffer = [Device newBufferWithBytes:Indices
				   length:sizeof(Indices) options:MTLResourceStorageModePrivate];

	NSError* Error = nil;
	NSString* Source = [NSString stringWithUTF8String:(char*)G_TriangleShader];
	id<MTLLibrary> Library = [Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction = [Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction = [Library newFunctionWithName:@"FragmentMain"];

	MTLRenderPipelineDescriptor* PipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	PipelineDesc.vertexFunction = VertexFunction;
	PipelineDesc.fragmentFunction = FragmentFunction;
	PipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	PipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	DepthStencilState = [Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	PipelineState = [Device newRenderPipelineStateWithDescriptor:PipelineDesc error:&Error];
}

f32 Angle = 0.0;

void R_Update(r_context *RenderContext)
{
	id<CAMetalDrawable> Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();
	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Drawable.texture;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(RenderContext, CGSizeMake(Drawable.texture.width, Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLCommandBuffer> CommandBuffer = [CommandQueue commandBuffer];

	Uniforms.ModelMatrix = M44FromXAngle(Angle);
	Angle += 0.01;
	Uniforms.ModelMatrix = MulM44FM44F(Uniforms.ModelMatrix, M44FTranslate(v3f(0, 2 * sinf(Angle), 0)));
	
	Uniforms.PerspectiveMatrix = M44FPerspective((f32)Drawable.texture.width/(f32)Drawable.texture.height, 100.0, 0.1, 10000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	MemoryCopy(UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:VertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:36
															indexType:MTLIndexTypeUInt32
															indexBuffer:IndexBuffer
															indexBufferOffset:0];
	[Encoder endEncoding];

	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];
}