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

void R_PushCommand(render_context *R, m_arena *Arena, r_command Command)
{
	r_command *NextCommand = (r_command*)ArenaPushZero(Arena, sizeof(r_command));
	NextCommand[0] = Command;
	if(!R->CommandsLast)
	{
		R->CommandsFirst = NextCommand;
		R->CommandsLast = NextCommand;
	}
	else
	{
		R->CommandsLast->Next = NextCommand;
		R->CommandsLast = NextCommand;
	}
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

id<MTLTexture> ResizeDepthTexture(render_context *R, CGSize Size)
{
	metal_context *Metal = R->Backend;
	if(Size.width != [Metal->DepthTexture width] || Size.height != [Metal->DepthTexture height] || Metal->DepthTexture == nil)
	{
		Metal->DepthTexture = nil;

		Metal->DepthTexture = MakeDepthTexture(R, Size);

		LogInfo("DepthTexture resized");
	}
	return Metal->DepthTexture;
}

metal_shader R_BloomShader(render_context *R)
{
	metal_context *Metal = (metal_context*)R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_BloomShader
									length:sizeof(G_BloomShader)
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
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_GlyphShader
											length:sizeof(G_GlyphShader)
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

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

metal_shader R_TriangleShader(render_context *R)
{
	metal_context *Metal = R->Backend;
	metal_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_TriangleShader
											length:sizeof(G_TriangleShader)
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

void R_Init(render_context *R, m_arena *Arena)
{
	metal_context* Metal = ArenaPushTypeZero(Arena, metal_context);
	R->Backend = Metal;

	Metal->Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	Metal->CommandQueue = [Metal->Device newCommandQueue];

	for(int ResourceIndex = 0; ResourceIndex < FRAMES_IN_FLIGHT; ResourceIndex++)
	{
		Metal->GlyphVertexBuffers[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(16)
						options:MTLResourceStorageModeShared];

		Metal->TriangleVertexBuffers[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(16)
						options:MTLResourceStorageModeShared];

		Metal->UniformBuffers[ResourceIndex] = [Metal->Device newBufferWithLength:sizeof(Uniforms)
						options:MTLResourceStorageModeShared];
	}

	Metal->BloomShader = R_BloomShader(R);
	Metal->GlyphShader = R_GlyphShader(R);
	Metal->TriangleShader = R_TriangleShader(R);
}

f32 Angle = 0.0;

void R_RenderPoints(render_context *R, id<MTLCommandBuffer> CommandBuffer, int VertexCount, int Offset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.storeAction = MTLStoreActionStore;
	

	Angle += 0.01;
	Uniforms.ModelMatrix = M44FromZAngle(Angle);
	Uniforms.ModelMatrix = M44F_IDENTITY;
	Uniforms.ProjectionMatrix = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	//Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 0), v3f(0, 0, 10), v3f(0, 1, 0));
	MemoryCopy(Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT].contents, &Uniforms, sizeof(Uniforms));
	
	
	Uniforms.ModelMatrix = M44F_IDENTITY;
	Uniforms.ProjectionMatrix = M44F_IDENTITY;
	Uniforms.ViewMatrix = M44F_IDENTITY;

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	id<MTLDepthStencilState> DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->TriangleShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->TriangleVertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:Offset vertexCount:VertexCount];



	[Encoder endEncoding];
}

void R_RenderTriangles(render_context *R, id<MTLCommandBuffer> CommandBuffer, int VertexCount, int Offset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	
	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.storeAction = MTLStoreActionStore;

	Uniforms.ModelMatrix = M44F_IDENTITY;
	Uniforms.ProjectionMatrix = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	Uniforms.ViewMatrix = M44F_IDENTITY;

	MemoryCopy(Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT].contents, &Uniforms, sizeof(Uniforms));

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	id<MTLDepthStencilState> DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->TriangleShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->TriangleVertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:Offset vertexCount:VertexCount];


	[Encoder endEncoding];
}

void R_RenderGlyphs(render_context *R, id<MTLCommandBuffer> CommandBuffer, int VertexCount, int Offset)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	Uniforms.ModelMatrix = M44F_IDENTITY;
	Uniforms.ProjectionMatrix = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));
	MemoryCopy(Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT].contents, &Uniforms, sizeof(Uniforms));

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->GlyphShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->GlyphVertexBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:1];
	[Encoder setFragmentTexture:Metal->FontAtlasTexture atIndex:0];
	[Encoder setFragmentSamplerState:Metal->SamplerState atIndex:0];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:Offset vertexCount:VertexCount];


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

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
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

	if(!Metal->BaseFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:Metal->Drawable.texture.width
														height:Metal->Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BaseFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	if(!Metal->BlurredFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:Metal->Drawable.texture.width
														height:Metal->Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BlurredFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}


	id<MTLCommandBuffer> CommandBuffer = [Metal->CommandQueue commandBuffer];
	R_ClearAllTextures(R, CommandBuffer);

	Angle += 0.01;

	int TotalGlyphVertices = 0;
	int GlyphVertexOffset = 0;

	int TotalTriangleVertices = 0;
	int TriangleVertexOffset = 0;

	int ResourceIndex = R->Frame % FRAMES_IN_FLIGHT;

	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{

		if(0){}
		else if(Command->Type == R_COMMAND_GLYPH)
		{	
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPushZero(FrameArena,
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
			MemoryCopy(Metal->GlyphVertexBuffers[ResourceIndex].contents + GlyphVertexOffset*sizeof(gpu_vertex) + TotalGlyphVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalGlyphVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_TRIANGLE)
		{
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPushZero(FrameArena,
															 sizeof(gpu_vertex)*Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
			}
			MemoryCopy(Metal->TriangleVertexBuffers[ResourceIndex].contents + TriangleVertexOffset*sizeof(gpu_vertex) + TotalTriangleVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalTriangleVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_POINTS)
		{
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPushZero(FrameArena,
															 sizeof(gpu_vertex)*Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
			}
			MemoryCopy(Metal->TriangleVertexBuffers[ResourceIndex].contents + TriangleVertexOffset*sizeof(gpu_vertex) + TotalTriangleVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalTriangleVertices += Command->VerticesCount;
		}

		if((Command->Next && Command->Next->Type != Command->Type) || (!Command->Next))
		{
			if(Command->Type == R_COMMAND_GLYPH)
			{
				R_RenderGlyphs(R, CommandBuffer, TotalGlyphVertices, GlyphVertexOffset);
				GlyphVertexOffset += TotalGlyphVertices;
				TotalGlyphVertices = 0;
			}
			else if(Command->Type == R_COMMAND_TRIANGLE)
			{
				R_RenderTriangles(R, CommandBuffer, TotalTriangleVertices, TriangleVertexOffset);
				TriangleVertexOffset += TotalTriangleVertices;
				TotalTriangleVertices = 0;
			}
			else if(Command->Type == R_COMMAND_POINTS)
			{
				R_RenderPoints(R, CommandBuffer, TotalTriangleVertices, TriangleVertexOffset);
				TriangleVertexOffset += TotalTriangleVertices;
				TotalTriangleVertices = 0;
			}
		}
	}

	R_RenderFullscreenBloom(R, CommandBuffer);

	[CommandBuffer presentDrawable:Metal->Drawable];
	[CommandBuffer commit];
	[CommandBuffer waitUntilCompleted];

	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;
}
