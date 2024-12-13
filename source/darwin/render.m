#include <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "sokol_app.h"
#include "os.h"

#include "shaders.h"


id<MTLCommandQueue> CommandQueue;
id<MTLDevice> Device;
id<MTLRenderPipelineState> PipelineState;
id<MTLDepthStencilState> DepthStencilState;
id<MTLTexture> DepthTexture;


id<MTLTexture> MakeDepthTexture(CGSize Size)
{
	MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float 
	                                                   width:Size.width
	                                                   height:Size.height mipmapped:NO];
	desc.usage = MTLTextureUsageRenderTarget;
	desc.storageMode = MTLStorageModePrivate;

	id<MTLTexture> Result = [Device newTextureWithDescriptor:desc];
	assert(Result != nil);
	return Result;
}

id<MTLTexture> ResizeDepthTexture(CGSize Size)
{
	assert(Size.width == 1280);
	assert(Size.height == 720);
	if(Size.width != [DepthTexture width] || Size.height != [DepthTexture height] || DepthTexture == nil)
	{
		DepthTexture = nil;

		DepthTexture = MakeDepthTexture(Size);
	}
	else
	{
		assert(DepthTexture != nil);
	}
	return DepthTexture;
}

void R_Init()
{
	Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	CommandQueue = [Device newCommandQueue];

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

void R_Update()
{
	id<CAMetalDrawable> Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();
	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Drawable.texture;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(CGSizeMake(Drawable.texture.width, Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLCommandBuffer> CommandBuffer = [CommandQueue commandBuffer];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:PipelineState];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[Encoder endEncoding];

	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];
}