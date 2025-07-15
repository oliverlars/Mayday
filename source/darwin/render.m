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


#include <stdalign.h>

#include "darwin/metal_render.h"

typedef struct uniforms
{
	m44f PVMMatrix;
} uniforms;


typedef struct gpu_vertex
{
	v4f Position;
	v4f Colour;
	v4f UV;
	f32 PointSize;
	f32 Padding[3];
}
gpu_vertex;

typedef struct gpu_glyph_vertex
{
	v4f Position;
	v4f Colour;
	v4f UV;
	u32 CurveStart;
	u32 CurveCount;
	f32 PointSize;
	f32 Padding;
}
gpu_glyph_vertex;

typedef struct gpu_line_vertex
{
	v4f Position;
	v4f Colour;
	v2f Start;
	v2f End;
	f32 Thickness;
	f32 Padding[3];
}
gpu_line_vertex;

typedef struct gpu_bezier_vertex
{
	v4f Position;
	v4f Colour;
	v2f Start;
	v2f Control;
	v2f End;
	f32 Thickness;
	f32 Padding;
}
gpu_bezier_vertex;

void R_UpdateCurves(render_context *Render, v2f *CurvePoints, int CurvePointsCount, int CurvePointsSoFar)
{

	metal_context *Metal = Render->Backend;
	if(!Metal->Curves)
	{
		Metal->Curves = ArenaPushArray(Render->Arena, v2f, MegaBytes(16));
	}
	MemoryCopy(Metal->Curves + CurvePointsSoFar,
					CurvePoints, CurvePointsCount*sizeof(v2f));
	Metal->TotalCurvesSoFar += CurvePointsCount;

}

// Dumb hack
void R_TextureFromBytes(render_context *R, u8 *Bytes, s32 Width, s32 Height)
{
	metal_context *Metal = R->Backend;
	if(!Metal->FontAtlasTexture)
	{
		MTLTextureDescriptor *TextureDescriptor = [MTLTextureDescriptor
													texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
													width:Width
													height:Height
													mipmapped:NO];
		TextureDescriptor.usage = MTLTextureUsageShaderRead;
		TextureDescriptor.storageMode = MTLStorageModeShared;
		Metal->FontAtlasTexture = [Metal->Device newTextureWithDescriptor:TextureDescriptor];
	}
	MTLRegion region = MTLRegionMake2D(0, 0, Width, Height);
	[Metal->FontAtlasTexture replaceRegion:region mipmapLevel:0 withBytes:Bytes bytesPerRow:Width];
}

id<MTLTexture> MakeDepthTexture(render_context *R, CGSize Size)
{
	metal_context *Metal = R->Backend;
	MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
													   width:Size.width
													   height:Size.height mipmapped:NO];
	Desc.usage = MTLTextureUsageRenderTarget;
	Desc.storageMode = MTLStorageModePrivate;

	id<MTLTexture> Result = [Metal->Device newTextureWithDescriptor:Desc];
	assert(Result != nil);
	return Result;
}

id<MTLTexture> ResizeTexture(render_context *R, id<MTLTexture> Texture, CGSize Size)
{
	metal_context *Metal = R->Backend;
	id<MTLTexture> Result = Texture;
	// TODO(Oliver): Delete the texture?? Memory leak
	if(Size.width != [Texture width] || Size.height != [Texture height] || Texture == nil)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Texture.pixelFormat
														width:Size.width
														height:Size.height mipmapped:NO];
		Desc.usage = Texture.usage;
		Desc.storageMode = Texture.storageMode;

		Result = [Metal->Device newTextureWithDescriptor:Desc];

		LogInfo("Texture resized");
	}
	return Result;
}

metal_shader R_BloomShader(render_context *R)
{
	metal_context *Metal = (metal_context*)R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_BloomShader.Data
									length:G_BloomShader.Length
									encoding:NSUTF8StringEncoding];
	Shader.Library                   = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction   = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction = [Shader.Library newFunctionWithName:@"FragmentMain"];

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

	Shader.PipelineState =  [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_GlyphShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_GlyphShader.Data
											length:G_GlyphShader.Length
											encoding:NSUTF8StringEncoding];
	Shader.Library                    = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction    = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction  = [Shader.Library newFunctionWithName:@"FragmentMain"];

	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

	MTLRenderPipelineDescriptor* PipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	PipelineDescriptor.vertexFunction = VertexFunction;
	PipelineDescriptor.fragmentFunction = FragmentFunction;
	PipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	PipelineDescriptor.colorAttachments[0].blendingEnabled = true;
	PipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	PipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	PipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	PipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;


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
	VertexDescriptor.attributes[3].format = MTLVertexFormatUInt;
	VertexDescriptor.attributes[3].offset = 3*sizeof(v4f);
	VertexDescriptor.attributes[3].bufferIndex = 0;
	VertexDescriptor.attributes[4].format = MTLVertexFormatUInt;
	VertexDescriptor.attributes[4].offset = 3*sizeof(v4f) + sizeof(u32);
	VertexDescriptor.attributes[4].bufferIndex = 0;
	VertexDescriptor.layouts[0].stride = sizeof(gpu_glyph_vertex);

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

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_TriangleShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_TriangleShader.Data
											length:G_TriangleShader.Length
											encoding:NSUTF8StringEncoding];
	Shader.Library                    = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction    = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction  = [Shader.Library newFunctionWithName:@"FragmentMain"];

	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

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
	VertexDescriptor.layouts[0].stride = sizeof(gpu_vertex);

	PipelineDescriptor.vertexDescriptor = VertexDescriptor;

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_PointShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_PointShader.Data
											length:G_PointShader.Length
											encoding:NSUTF8StringEncoding];
	Shader.Library                    = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction    = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction  = [Shader.Library newFunctionWithName:@"FragmentMain"];

	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

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
	VertexDescriptor.layouts[0].stride = sizeof(gpu_vertex);

	PipelineDescriptor.vertexDescriptor = VertexDescriptor;

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_LineShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_LineShader.Data
											length:G_LineShader.Length
											encoding:NSUTF8StringEncoding];
	Shader.Library                    = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction    = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction  = [Shader.Library newFunctionWithName:@"FragmentMain"];

	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

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
	VertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
	VertexDescriptor.attributes[2].offset = sizeof(v4f)*2;
	VertexDescriptor.attributes[2].bufferIndex = 0;
	VertexDescriptor.attributes[3].format = MTLVertexFormatFloat2;
	VertexDescriptor.attributes[3].offset = sizeof(v4f)*2 + sizeof(v2f);
	VertexDescriptor.attributes[3].bufferIndex = 0;
	VertexDescriptor.attributes[4].format = MTLVertexFormatFloat;
	VertexDescriptor.attributes[4].offset = sizeof(v4f)*2 + sizeof(v2f)*2;
	VertexDescriptor.attributes[4].bufferIndex = 0;
	VertexDescriptor.layouts[0].stride = sizeof(gpu_line_vertex);

	PipelineDescriptor.vertexDescriptor = VertexDescriptor;

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_BezierShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_BezierShader.Data
											length:G_BezierShader.Length
											encoding:NSUTF8StringEncoding];
	Shader.Library                    = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction    = [Shader.Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction  = [Shader.Library newFunctionWithName:@"FragmentMain"];

	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

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
	VertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
	VertexDescriptor.attributes[2].offset = sizeof(v4f)*2;
	VertexDescriptor.attributes[2].bufferIndex = 0;
	VertexDescriptor.attributes[3].format = MTLVertexFormatFloat2;
	VertexDescriptor.attributes[3].offset = sizeof(v4f)*2 + sizeof(v2f);
	VertexDescriptor.attributes[3].bufferIndex = 0;
	VertexDescriptor.attributes[4].format = MTLVertexFormatFloat2;
	VertexDescriptor.attributes[4].offset = sizeof(v4f)*2 + sizeof(v2f)*2;
	VertexDescriptor.attributes[4].bufferIndex = 0;
	VertexDescriptor.attributes[5].format = MTLVertexFormatFloat;
	VertexDescriptor.attributes[5].offset = sizeof(v4f)*2 + sizeof(v2f)*3;
	VertexDescriptor.attributes[5].bufferIndex = 0;
	VertexDescriptor.layouts[0].stride = sizeof(gpu_bezier_vertex);

	PipelineDescriptor.vertexDescriptor = VertexDescriptor;

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

void R_Init(render_context *R, m_arena *Arena, v2i Size)
{
	metal_context* Metal = ArenaPushArrayZero(Arena, metal_context, 1);
	R->Backend = Metal;

	Metal->Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	Metal->CommandQueue = [Metal->Device newCommandQueue];

	for(int ResourceIndex = 0; ResourceIndex < FRAMES_IN_FLIGHT; ResourceIndex++)
	{
		Metal->VertexBuffers[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(128)
						options:MTLResourceStorageModeShared];

		Metal->UniformBuffers[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(128)
						options:MTLResourceStorageModeShared];

		Metal->CurveBuffer[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(32)
			options:MTLResourceStorageModeShared];
	}

	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:Size.X
														height:Size.Y mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BaseFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:Size.X
														height:Size.Y mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;

		Metal->BlurredFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
														width:Size.X
														height:Size.Y mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;;
		Desc.storageMode = MTLStorageModePrivate;

		Metal->DepthTexture = [Metal->Device newTextureWithDescriptor:Desc];
	}

	Metal->BloomShader = R_BloomShader(R);
	Metal->GlyphShader = R_GlyphShader(R);
	Metal->TriangleShader = R_TriangleShader(R);
	Metal->PointShader = R_PointShader(R);
	Metal->LineShader = R_LineShader(R);
	Metal->BezierShader = R_BezierShader(R);

	Metal->InFlightSemaphore = dispatch_semaphore_create(FRAMES_IN_FLIGHT);
}

void R_RenderPoints(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = Metal->DepthTexture;
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.storeAction = MTLStoreActionStore;

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	id<MTLDepthStencilState> DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->PointShader.PipelineState];
	[Encoder setVertexBuffer:Metal->VertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:VertexOffset atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:UniformOffset atIndex:1];

	MTLScissorRect ScissorRect = {
		.x = Scissor.X,
		.y = Scissor.Y,
		.width = Scissor.Z,
		.height = Scissor.W
	};

	[Encoder setScissorRect:ScissorRect];

	[Encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:VertexCount];


	[Encoder endEncoding];
}

void R_RenderLines(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	
	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->LineShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];;

	MTLScissorRect ScissorRect = {
		.x = Scissor.X,
		.y = Scissor.Y,
		.width = Scissor.Z,
		.height = Scissor.W
	};

	//[Encoder setScissorRect:ScissorRect];

	[Encoder setVertexBuffer:Metal->VertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:VertexOffset atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:UniformOffset atIndex:1];

	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];

	[Encoder endEncoding];
}

void R_RenderBeziers(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	
	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->BezierShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];;

	MTLScissorRect ScissorRect = {
		.x = Scissor.X,
		.y = Scissor.Y,
		.width = Scissor.Z,
		.height = Scissor.W
	};

	[Encoder setScissorRect:ScissorRect];

	[Encoder setVertexBuffer:Metal->VertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:VertexOffset atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:UniformOffset atIndex:1];

	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];

	[Encoder endEncoding];
}

void R_RenderTriangles(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = Metal->DepthTexture;
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.storeAction = MTLStoreActionStore;

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	id<MTLDepthStencilState> DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->TriangleShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->VertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:VertexOffset atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:UniformOffset atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];

	MTLScissorRect ScissorRect = {
		.x = Scissor.X,
		.y = Scissor.Y,
		.width = Scissor.Z,
		.height = Scissor.W
	};

	[Encoder setScissorRect:ScissorRect];

	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];


	[Encoder endEncoding];
}

void R_RenderGlyphs(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->GlyphShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];

	MTLScissorRect ScissorRect = {
		.x = Scissor.X,
		.y = Scissor.Y,
		.width = Scissor.Z,
		.height = Scissor.W
	};

	[Encoder setScissorRect:ScissorRect];

	[Encoder setVertexBuffer:Metal->VertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:VertexOffset atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:UniformOffset atIndex:1];
	[Encoder setFragmentTexture:Metal->FontAtlasTexture atIndex:0];
	[Encoder setFragmentBuffer:Metal->CurveBuffer[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:0];
	[Encoder setFragmentSamplerState:Metal->SamplerState atIndex:0];

	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];


	[Encoder endEncoding];
}

void R_RenderFullscreenBloom(render_context *R, id<MTLCommandBuffer> CommandBuffer)
{
	metal_context *Metal = R->Backend;

	MPSImageGaussianBlur *GaussianBlur = [[MPSImageGaussianBlur alloc] initWithDevice:Metal->Device sigma:10];

	[GaussianBlur encodeToCommandBuffer:CommandBuffer
				  sourceTexture:Metal->BaseFrameBuffer
				  destinationTexture:Metal->BlurredFrameBuffer];

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->Drawable.texture;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->BloomShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setFragmentTexture:Metal->BaseFrameBuffer atIndex:0];
	[Encoder setFragmentTexture:Metal->BlurredFrameBuffer atIndex:1];
	[Encoder setFragmentSamplerState:Metal->BloomSamplerState atIndex:0];

	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

	[Encoder endEncoding];
}

void R_ClearAllTextures(render_context *R, id<MTLCommandBuffer> CommandBuffer)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->Drawable.texture;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	PassDesc.colorAttachments[1].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[1].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[1].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[1].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	PassDesc.colorAttachments[2].texture = Metal->BlurredFrameBuffer;
	PassDesc.colorAttachments[2].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[2].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[2].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = Metal->DepthTexture;
	PassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	PassDesc.depthAttachment.storeAction = MTLStoreActionStore;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder endEncoding];
}

void R_Update(render_context *R, m_arena *FrameArena)
{
	metal_context *Metal = R->Backend;
	Metal->Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();

	dispatch_semaphore_wait(Metal->InFlightSemaphore, DISPATCH_TIME_FOREVER);

	Metal->DepthTexture = ResizeTexture(R, Metal->DepthTexture,
									CGSizeMake(Metal->Drawable.texture.width,
												Metal->Drawable.texture.height));

	Metal->BaseFrameBuffer = ResizeTexture(R, Metal->BaseFrameBuffer,
									CGSizeMake(Metal->Drawable.texture.width,
												Metal->Drawable.texture.height));

	Metal->BlurredFrameBuffer = ResizeTexture(R, Metal->BlurredFrameBuffer,
									CGSizeMake(Metal->Drawable.texture.width,
												Metal->Drawable.texture.height));

	id<MTLCommandBuffer> CommandBuffer = [Metal->CommandQueue commandBuffer];

	R_ClearAllTextures(R, CommandBuffer);


	MemoryCopy(Metal->CurveBuffer[(R->Frame)% 3].contents,
					Metal->Curves, Metal->TotalCurvesSoFar*sizeof(v2f));


	int TotalVertices = 0;
	int VertexOffsetInBytes = 0;

	int ResourceIndex = R->Frame % FRAMES_IN_FLIGHT;

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	int UniformOffset = 0;

	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{

		if(0){}
		else if(Command->Type == R_COMMAND_GLYPH)
		{
			gpu_glyph_vertex *GPUVertices = (gpu_glyph_vertex*)ArenaPushArrayZero(Scratch.Arena,
																	  gpu_glyph_vertex, Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				u32 CurveStart = Command->Vertices[V].CurveStart;
				u32 CurveCount = Command->Vertices[V].CurveCount;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
				GPUVertices[V].CurveStart = CurveStart;
				GPUVertices[V].CurveCount = CurveCount;
				GPUVertices[V].UV = v4f(UV.X, UV.Y, 0, 0);
			}
			MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + VertexOffsetInBytes + TotalVertices*sizeof(gpu_glyph_vertex),
					GPUVertices, sizeof(gpu_glyph_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_TRIANGLE)
		{
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPushArrayZero(Scratch.Arena,
															 	 	  gpu_vertex, Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
			}
			MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + VertexOffsetInBytes + TotalVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_POINTS)
		{
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPushArrayZero(Scratch.Arena,
																	  gpu_vertex, Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				f32 PointSize = Command->Vertices[V].PointSize;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
				GPUVertices[V].PointSize = PointSize;
			}
			MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + VertexOffsetInBytes + TotalVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_LINES)
		{
			gpu_line_vertex *GPUVertices = (gpu_line_vertex*)ArenaPushArrayZero(Scratch.Arena,
															 		  gpu_line_vertex, Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v4f Colour = Command->Vertices[V].Colour;
				v2f Start = Command->Vertices[V].Start;
				v2f End = Command->Vertices[V].End;
				f32 Thickness = Command->Vertices[V].Thickness;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
				GPUVertices[V].Start = Start;
				GPUVertices[V].End = End;
				GPUVertices[V].Thickness = Thickness;
			}
			MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + VertexOffsetInBytes + TotalVertices*sizeof(gpu_line_vertex),
					GPUVertices, sizeof(gpu_line_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		} else if(Command->Type == R_COMMAND_BEZIER)
		{
			gpu_bezier_vertex *GPUVertices = (gpu_bezier_vertex*)ArenaPushArrayZero(Scratch.Arena,
															 		  gpu_bezier_vertex, Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v4f Colour = Command->Vertices[V].Colour;
				v2f Start = Command->Vertices[V].Start;
				v2f Control = Command->Vertices[V].Control;
				v2f End = Command->Vertices[V].End;
				f32 Thickness = Command->Vertices[V].Thickness;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
				GPUVertices[V].Start = Start;
				GPUVertices[V].Control = Control;
				GPUVertices[V].End = End;
				GPUVertices[V].Thickness = Thickness;
			}
			MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + VertexOffsetInBytes + TotalVertices*sizeof(gpu_bezier_vertex),
					GPUVertices, sizeof(gpu_bezier_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}

		if((Command->Next && Command->Next->Type != Command->Type) ||
		   (Command->Next && !MemoryEqual(&Command->Transform, &Command->Next->Transform, sizeof(m44f))) ||
		   (!Command->Next))
		{
			uniforms Uniforms = { Command->Transform };
			MemoryCopy(Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT].contents + UniformOffset*sizeof(uniforms), &Uniforms, sizeof(Uniforms));

			if(Command->Type == R_COMMAND_GLYPH)
			{
				R_RenderGlyphs(R, CommandBuffer, Command->Scissor, TotalVertices, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
				VertexOffsetInBytes += TotalVertices*sizeof(gpu_vertex);
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_TRIANGLE)
			{
				R_RenderTriangles(R, CommandBuffer, Command->Scissor, TotalVertices, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
				VertexOffsetInBytes += TotalVertices*sizeof(gpu_vertex);
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_POINTS)
			{
				R_RenderPoints(R, CommandBuffer, Command->Scissor, TotalVertices, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
				VertexOffsetInBytes += TotalVertices*sizeof(gpu_vertex);
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_LINES)
			{
				R_RenderLines(R, CommandBuffer, Command->Scissor, TotalVertices, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
				VertexOffsetInBytes += TotalVertices*sizeof(gpu_line_vertex);
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_BEZIER)
			{
				R_RenderBeziers(R, CommandBuffer, Command->Scissor, TotalVertices, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
				VertexOffsetInBytes += TotalVertices*sizeof(gpu_bezier_vertex);
				TotalVertices = 0;
			}

			UniformOffset += 1;
		}
	}

	R_RenderFullscreenBloom(R, CommandBuffer);
	[CommandBuffer presentDrawable:Metal->Drawable];



	__block dispatch_semaphore_t BlockSemaphore = Metal->InFlightSemaphore;
	[CommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
	{
		dispatch_semaphore_signal(BlockSemaphore);
	}];

	[CommandBuffer commit];
	[CommandBuffer waitUntilCompleted];


	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;

	R->TransformCount = 0;
	R->ScissorCount = 0;

	ScratchEnd(Scratch);
}
