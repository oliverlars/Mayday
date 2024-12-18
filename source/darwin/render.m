#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include <assert.h>

#include "memory.h"
#include "misc.h"
#include "sokol_app.h"

#include "os.h"
#include "maths.h"
#include "render.h"
#include "shaders.h"
#include "log.h"

#include "darwin/metal_render.h"

typedef struct uniforms
{
	m44f ProjectionMatrix;
	m44f ViewMatrix;
	m44f ModelMatrix;
} uniforms;

uniforms Uniforms;

typedef struct gpu_vertex
{
	v4f Position;
	v4f Colour;
	v4f UV;
}
gpu_vertex;

void R_PushCommand(r_context *R_Context, memory_arena *Arena, r_command Command)
{
	r_command *NextCommand = (r_command*)ArenaPushZero(Arena, sizeof(r_command));
	NextCommand[0] = Command;
	if(!R_Context->CommandsLast)
	{
		R_Context->CommandsFirst = NextCommand;
		R_Context->CommandsLast = NextCommand;
	}
	else
	{
		R_Context->CommandsLast->Next = NextCommand;
		R_Context->CommandsLast = NextCommand;
	}
}

// Dumb hack
void R_TextureFromBytes(r_context *R_Context, u8 *Bytes, s32 Width, s32 Height)
{
	metal_context *Metal = R_Context->Backend;
	MTLTextureDescriptor *TextureDescriptor = [MTLTextureDescriptor
											   texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
											   width:Width
											   height:Height
											   mipmapped:NO];
	TextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	Metal->FontAtlasTexture = [Metal->Device newTextureWithDescriptor:TextureDescriptor];
	MTLRegion region = MTLRegionMake2D(0, 0, Width, Height);
	[Metal->FontAtlasTexture replaceRegion:region mipmapLevel:0 withBytes:Bytes bytesPerRow:Width];
}

id<MTLTexture> MakeDepthTexture(r_context *R_Context, CGSize Size)
{
	metal_context *Metal = R_Context->Backend;
	MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float 
													   width:Size.width
													   height:Size.height mipmapped:NO];
	Desc.usage = MTLTextureUsageRenderTarget;
	Desc.storageMode = MTLStorageModeMemoryless;

	id<MTLTexture> Result = [Metal->Device newTextureWithDescriptor:Desc];
	assert(Result != nil);
	return Result;
}

id<MTLTexture> ResizeDepthTexture(r_context *R_Context, CGSize Size)
{
	metal_context *Metal = R_Context->Backend;
	if(Size.width != [Metal->DepthTexture width] || Size.height != [Metal->DepthTexture height] || Metal->DepthTexture == nil)
	{
		Metal->DepthTexture = nil;

		Metal->DepthTexture = MakeDepthTexture(R_Context, Size);

		LogInfo(R_Context->Logger, "DepthTexture resized");
	}
	return Metal->DepthTexture;
}

void R_Init(r_context *R_Context)
{
	metal_context* Metal = ArenaPushTypeZero(R_Context->Arena, metal_context);
	R_Context->Backend = Metal;

	Metal->Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	Metal->CommandQueue = [Metal->Device newCommandQueue];

	Metal->VertexBuffer = [Metal->Device newBufferWithLength:MegaBytes(16)*sizeof(gpu_vertex)
					options:MTLResourceStorageModeShared];

	Metal->UniformBuffer = [Metal->Device newBufferWithLength:sizeof(Uniforms)
					 options:MTLResourceStorageModeShared];

	{
		NSError* Error = nil;
		NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_TriangleShader
											 length:sizeof(G_TriangleShader)
											 encoding:NSUTF8StringEncoding];
		id<MTLLibrary> TriangleLibrary = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
		id<MTLFunction> VertexFunction = [TriangleLibrary newFunctionWithName:@"VertexMain"];
		id<MTLFunction> FragmentFunction = [TriangleLibrary newFunctionWithName:@"FragmentMain"];

		NSLog(@"Failed to create library: %@", Error);

		MTLRenderPipelineDescriptor* PipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		PipelineDescriptor.vertexFunction = VertexFunction;
		PipelineDescriptor.fragmentFunction = FragmentFunction;
		PipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		PipelineDescriptor.colorAttachments[0].blendingEnabled = true;
		PipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		PipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		PipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
		PipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		PipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;


		MTLVertexDescriptor *VertexDescriptor = [[MTLVertexDescriptor alloc] init];
		VertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
		VertexDescriptor.attributes[0].offset = 0;
		VertexDescriptor.attributes[0].bufferIndex = 0;
		VertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
		VertexDescriptor.attributes[1].offset = sizeof(v4f);
		VertexDescriptor.attributes[1].bufferIndex = 0;
		VertexDescriptor.attributes[2].format = MTLVertexFormatFloat4;
		VertexDescriptor.attributes[2].offset = 2*sizeof(v4f);
		VertexDescriptor.attributes[2].bufferIndex = 0;
		VertexDescriptor.layouts[0].stride = sizeof(gpu_vertex); 

		PipelineDescriptor.vertexDescriptor = VertexDescriptor;

		MTLSamplerDescriptor *SamplerDescriptor = [[MTLSamplerDescriptor alloc] init];
		SamplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
		SamplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
		SamplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;
		SamplerDescriptor.lodMinClamp = 0.0;
		SamplerDescriptor.lodMaxClamp = FLT_MAX;
		SamplerDescriptor.normalizedCoordinates = YES;
		Metal->SamplerState = [Metal->Device newSamplerStateWithDescriptor:SamplerDescriptor];

		MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
		DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
		DepthStencilDescriptor.depthWriteEnabled = YES;
		Metal->DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

		Metal->TrianglePipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	}

	{
		NSError* Error = nil;
		NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_BloomShader
										length:sizeof(G_BloomShader)
										encoding:NSUTF8StringEncoding];
		id<MTLLibrary> BloomLibrary = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
		id<MTLFunction> VertexFunction = [BloomLibrary newFunctionWithName:@"VertexMain"];
		id<MTLFunction> FragmentFunction = [BloomLibrary newFunctionWithName:@"FragmentMain"];

		MTLRenderPipelineDescriptor* PipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
		PipelineDescriptor.vertexFunction = VertexFunction;
		PipelineDescriptor.fragmentFunction = FragmentFunction;
		PipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

		MTLSamplerDescriptor *SamplerDescriptor = [[MTLSamplerDescriptor alloc] init];
		SamplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
		SamplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
		SamplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
		SamplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;
		SamplerDescriptor.lodMinClamp = 0.0;
		SamplerDescriptor.lodMaxClamp = FLT_MAX;
		SamplerDescriptor.normalizedCoordinates = YES;
		Metal->BloomSamplerState = [Metal->Device newSamplerStateWithDescriptor:SamplerDescriptor];

		Metal->BloomPipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	}

}

f32 Angle = 0.0;

void R_Update(r_context *R_Context)
{
	metal_context *Metal = R_Context->Backend;

	id<CAMetalDrawable> Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();
	CAMetalLayer *MetalLayer = (CAMetalLayer *)Drawable.layer;
	MetalLayer.framebufferOnly = NO;

	if(!Metal->BaseFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Drawable.texture.pixelFormat 
														width:Drawable.texture.width
														height:Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BaseFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	if(!Metal->BlurredFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Drawable.texture.pixelFormat 
														width:Drawable.texture.width
														height:Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BlurredFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.05,0.05,0.05,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R_Context, CGSizeMake(Drawable.texture.width, Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLCommandBuffer> CommandBuffer = [Metal->CommandQueue commandBuffer];

	Uniforms.ModelMatrix = M44FromXAngle(Angle);
	Angle += 0.01;
	Uniforms.ModelMatrix = MulM44FM44F(Uniforms.ModelMatrix, M44FTranslate(v3f(0, 2 * sinf(Angle), 0)));
	
	//Uniforms.PerspectiveMatrix = M44FPerspective((f32)Drawable.texture.width/(f32)Drawable.texture.height, 100.0, 0.1, 10000.0);
	Uniforms.ProjectionMatrix = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	MemoryCopy(Metal->UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	// TODO(Oliver): Make this a proper construct
	u64 ArenaResetTo = R_Context->Arena->At;

	int TotalVertices = 0;
	for(r_command *Command = R_Context->CommandsFirst; Command != Null; Command = Command->Next)
	{
		gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPush(R_Context->Arena,
														 sizeof(gpu_vertex)*Command->VerticesCount);
		for(int V = 0; V < Command->VerticesCount; V++)
		{
			v4f P = Command->Vertices[V].Position;
			v2f UV = Command->Vertices[V].UV;
			v4f Colour = Command->Vertices[V].Colour;
			GPUVertices[V].Position = P;
			GPUVertices[V].Colour = Colour;
			GPUVertices[V].UV = v4f(UV.X, UV.Y, 0, 0);
		}
		MemoryCopy(Metal->VertexBuffer.contents + TotalVertices*sizeof(gpu_vertex),
				   GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
		TotalVertices += Command->VerticesCount;
	}

	R_Context->Arena->At = ArenaResetTo;

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->TrianglePipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->VertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:Metal->DepthStencilState];
	[Encoder setFragmentTexture:Metal->FontAtlasTexture atIndex:0];
	[Encoder setFragmentSamplerState:Metal->SamplerState atIndex:0];
	// [Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:36
	// 														indexType:MTLIndexTypeUInt32
	// 														indexBuffer:IndexBuffer
	// 														indexBufferOffset:0];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:TotalVertices];


	[Encoder endEncoding];

	MPSImageGaussianBlur *GaussianBlur = [[MPSImageGaussianBlur alloc] initWithDevice:Metal->Device sigma:20];

	[GaussianBlur encodeToCommandBuffer:CommandBuffer
				  sourceTexture:Metal->BaseFrameBuffer
				  destinationTexture:Metal->BlurredFrameBuffer];


	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];

	[CommandBuffer waitUntilCompleted];

	R_Context->CommandsFirst = Null;
	R_Context->CommandsLast = Null;
}