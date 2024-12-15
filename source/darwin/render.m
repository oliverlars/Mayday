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


id<MTLCommandQueue> CommandQueue;
id<MTLDevice> Device;
id<MTLRenderPipelineState> PipelineState;
id<MTLDepthStencilState> DepthStencilState;
id<MTLTexture> DepthTexture;
id<MTLBuffer> VertexBuffer;
id<MTLBuffer> UniformBuffer;

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

vertex Vertices[36];

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
	Vertices[0] =  (vertex){{-1, -1,  1, 1}, {1,0,0,1}};
	Vertices[1] =  (vertex){{ 1,  1,  1, 1}, {1,0,0,1}};
	Vertices[2] =  (vertex){{-1,  1,  1, 1}, {1,0,0,1}};
	Vertices[3] =  (vertex){{-1, -1,  1, 1}, {1,0,0,1}};
	Vertices[4] =  (vertex){{ 1, -1,  1, 1}, {1,0,0,1}};
	Vertices[5] =  (vertex){{ 1,  1,  1, 1}, {1,0,0,1}};

	Vertices[6] =  (vertex){{-1, -1, -1, 1}, {0,1,0,1}};
	Vertices[7] =  (vertex){{-1,  1, -1, 1}, {0,1,0,1}};
	Vertices[8] =  (vertex){{ 1,  1, -1, 1}, {0,1,0,1}};
	Vertices[9] =  (vertex){{-1, -1, -1, 1}, {0,1,0,1}};
	Vertices[10] = (vertex){{ 1,  1, -1, 1}, {0,1,0,1}};
	Vertices[11] = (vertex){{ 1, -1, -1, 1}, {0,1,0,1}};

	Vertices[12] = (vertex){{-1,  1,  1, 1}, {0,0,1,1}};
	Vertices[13] = (vertex){{ 1,  1, -1, 1}, {0,0,1,1}};
	Vertices[14] = (vertex){{-1,  1, -1, 1}, {0,0,1,1}};
	Vertices[15] = (vertex){{-1,  1,  1, 1}, {0,0,1,1}};
	Vertices[16] = (vertex){{ 1,  1,  1, 1}, {0,0,1,1}};
	Vertices[17] = (vertex){{ 1,  1, -1, 1}, {0,0,1,1}};

	Vertices[18] = (vertex){{-1, -1, -1, 1}, {1,1,0,1}};
	Vertices[19] = (vertex){{ 1, -1,  1, 1}, {1,1,0,1}};
	Vertices[20] = (vertex){{-1, -1,  1, 1}, {1,1,0,1}};
	Vertices[21] = (vertex){{-1, -1, -1, 1}, {1,1,0,1}};
	Vertices[22] = (vertex){{ 1, -1, -1, 1}, {1,1,0,1}};
	Vertices[23] = (vertex){{ 1, -1,  1, 1}, {1,1,0,1}};

	Vertices[24] = (vertex){{-1, -1, -1, 1}, {0,1,1,1}};
	Vertices[25] = (vertex){{-1,  1,  1, 1}, {0,1,1,1}};
	Vertices[26] = (vertex){{-1,  1, -1, 1}, {0,1,1,1}};
	Vertices[27] = (vertex){{-1, -1, -1, 1}, {0,1,1,1}};
	Vertices[28] = (vertex){{-1, -1,  1, 1}, {0,1,1,1}};
	Vertices[29] = (vertex){{-1,  1,  1, 1}, {0,1,1,1}};

	Vertices[30] = (vertex){{ 1, -1, -1, 1}, {1,0,1,1}};
	Vertices[31] = (vertex){{ 1,  1, -1, 1}, {1,0,1,1}};
	Vertices[32] = (vertex){{ 1,  1,  1, 1}, {1,0,1,1}};
	Vertices[33] = (vertex){{ 1, -1, -1, 1}, {1,0,1,1}};
	Vertices[34] = (vertex){{ 1,  1,  1, 1}, {1,0,1,1}};
	Vertices[35] = (vertex){{ 1, -1,  1, 1}, {1,0,1,1}};


	Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	CommandQueue = [Device newCommandQueue];

	VertexBuffer = [Device newBufferWithBytes:Vertices
	                length:sizeof(Vertices)
	                options:MTLResourceStorageModePrivate];

	UniformBuffer = [Device newBufferWithLength:sizeof(Uniforms)
	                 options:MTLResourceStorageModePrivate];

	NSError* Error = nil;
	NSString* Source = [NSString stringWithUTF8String:(char*)TriangleShader];
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
	
	Uniforms.PerspectiveMatrix = M44FPerspective((f32)Drawable.texture.width/(f32)Drawable.texture.height, 30.0, 0.1, 10000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	MemoryCopy(UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:VertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36];
	[Encoder endEncoding];

	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];
}