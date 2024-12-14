#include "log.h"
#include <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#import <QuartzCore/QuartzCore.h>

#include "sokol_app.h"
#include "os.h"

#include "maths.h"

#include "render.h"

#include "shaders.h"


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
	m44f RotationMatrix;
} uniforms;

vertex Vertices[3];

uniforms Uniforms;


id<MTLTexture> MakeDepthTexture(r_context *RenderContext, CGSize Size)
{
	MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float 
	                                                   width:Size.width
	                                                   height:Size.height mipmapped:NO];
	Desc.usage = MTLTextureUsageRenderTarget;
	Desc.storageMode = MTLStorageModePrivate;

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

	Vertices[0] = (vertex)
	{
		v4f(0, 1, 0, 1),
		v4f(1, 0, 0, 1)
	};

	Vertices[1] = (vertex)
	{
		v4f(-1, -1, 0, 1),
		v4f(0, 1, 0, 1)
	};

	Vertices[2] = (vertex)
	{
		v4f(1, -1, 0, 1),
		v4f(0, 0, 1, 1)
	};

	Uniforms.RotationMatrix.E[0][0] = 1;
	Uniforms.RotationMatrix.E[1][1] = 1;
	Uniforms.RotationMatrix.E[2][2] = 1;
	Uniforms.RotationMatrix.E[3][3] = 1;

	Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	CommandQueue = [Device newCommandQueue];

	VertexBuffer = [Device newBufferWithBytes:Vertices
	                length:sizeof(Vertices)
	                options:MTLResourceOptionCPUCacheModeDefault];

	UniformBuffer = [Device newBufferWithLength:sizeof(Uniforms)
	                 options:MTLResourceOptionCPUCacheModeDefault];

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

	Uniforms.RotationMatrix = M44FromZAngle(Angle);
	Angle += 0.1;
	MemoryCopy(UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:PipelineState];
	[Encoder setVertexBuffer:VertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[Encoder endEncoding];

	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];
}