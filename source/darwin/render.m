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

void R_PushCommand(render_context *R, memory_arena *Arena, r_command Command)
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

		LogInfo(R->Logger, "DepthTexture resized");
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
		LogError(R->Logger, (char*)Error.localizedDescription.cString);
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
		LogError(R->Logger, (char*)Error.localizedDescription.cString);
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

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	Metal->DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	Shader.PipelineState = [Metal->Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];
	return Shader;
}

void R_Init(render_context *R)
{
	metal_context* Metal = ArenaPushTypeZero(R->Arena, metal_context);
	R->Backend = Metal;

	Metal->Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	Metal->CommandQueue = [Metal->Device newCommandQueue];

	Metal->GlyphVertexBuffer = [Metal->Device newBufferWithLength:MegaBytes(8)*sizeof(gpu_vertex)
					options:MTLResourceStorageModeShared];

	Metal->TriangleVertexBuffer = [Metal->Device newBufferWithLength:MegaBytes(8)*sizeof(gpu_vertex)
					options:MTLResourceStorageModeShared];

	Metal->UniformBuffer = [Metal->Device newBufferWithLength:sizeof(Uniforms)
					 options:MTLResourceStorageModeShared];


	Metal->BloomShader = R_BloomShader(R);
	Metal->GlyphShader = R_GlyphShader(R);
	Metal->TriangleShader = R_TriangleShader(R);

}

f32 Angle = 0.0;


void R_RenderTriangles(render_context *R, id<MTLCommandBuffer> CommandBuffer, int VertexCount)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.clearDepth = 1.0;


	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->TriangleShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->TriangleVertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:Metal->DepthStencilState];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];


	[Encoder endEncoding];
}

void R_RenderGlyphs(render_context *R, id<MTLCommandBuffer> CommandBuffer, int VertexCount)
{
	metal_context *Metal = R->Backend;

	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.clearDepth = 1.0;


	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:Metal->GlyphShader.PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:Metal->GlyphVertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:Metal->UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:Metal->DepthStencilState];
	[Encoder setFragmentTexture:Metal->FontAtlasTexture atIndex:0];
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
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
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
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	PassDesc.colorAttachments[1].texture = Metal->BaseFrameBuffer;
	PassDesc.colorAttachments[1].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[1].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);
	PassDesc.colorAttachments[2].texture = Metal->BlurredFrameBuffer;
	PassDesc.colorAttachments[2].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[2].clearColor = MTLClearColorMake(0.0,0.0,0.0,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R, CGSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder endEncoding];
}

void R_Update(render_context *R)
{
	metal_context *Metal = R->Backend;
	Metal->Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();

	CAMetalLayer *MetalLayer = (CAMetalLayer *)Metal->Drawable.layer;
	MetalLayer.framebufferOnly = NO;

	if(!Metal->BaseFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Metal->Drawable.texture.pixelFormat 
														width:Metal->Drawable.texture.width
														height:Metal->Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BaseFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}

	if(!Metal->BlurredFrameBuffer)
	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Metal->Drawable.texture.pixelFormat 
														width:Metal->Drawable.texture.width
														height:Metal->Drawable.texture.height mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
		Desc.storageMode = MTLStorageModePrivate;


		Metal->BlurredFrameBuffer = [Metal->Device newTextureWithDescriptor:Desc];
	}


	id<MTLCommandBuffer> CommandBuffer = [Metal->CommandQueue commandBuffer];


	R_ClearAllTextures(R, CommandBuffer);

	Uniforms.ModelMatrix = M44FromXAngle(Angle);
	Angle += 0.01;
	Uniforms.ModelMatrix = MulM44FM44F(Uniforms.ModelMatrix, M44FTranslate(v3f(0, 2 * sinf(Angle), 0)));
	
	Uniforms.ProjectionMatrix = M44FOrthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	MemoryCopy(Metal->UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	// TODO(Oliver): Make this a proper construct
	u64 ArenaResetTo = R->Arena->At;

	int TotalVertices = 0;
	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{

		if(0){}
		else if(Command->Type == R_COMMAND_GLYPH)
		{	
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPush(R->Arena,
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
			MemoryCopy(Metal->GlyphVertexBuffer.contents + TotalVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_TRIANGLE)
		{
			gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPush(R->Arena,
															 sizeof(gpu_vertex)*Command->VerticesCount);
			for(int V = 0; V < Command->VerticesCount; V++)
			{
				v4f P = Command->Vertices[V].Position;
				v2f UV = Command->Vertices[V].UV;
				v4f Colour = Command->Vertices[V].Colour;
				GPUVertices[V].Position = P;
				GPUVertices[V].Colour = Colour;
			}
			MemoryCopy(Metal->TriangleVertexBuffer.contents + TotalVertices*sizeof(gpu_vertex),
					GPUVertices, sizeof(gpu_vertex)*Command->VerticesCount);
			TotalVertices += Command->VerticesCount;
		}

		if((Command->Next && Command->Next->Type != Command->Type) || (!Command->Next))
		{
			if(Command->Type == R_COMMAND_GLYPH)
			{
				R_RenderGlyphs(R, CommandBuffer, TotalVertices);
			}
			else if(Command->Type == R_COMMAND_TRIANGLE)
			{
				R_RenderTriangles(R, CommandBuffer, TotalVertices);
			}
			TotalVertices = 0;
		}
	}

	R->Arena->At = ArenaResetTo;

	R_RenderFullscreenBloom(R, CommandBuffer);

	[CommandBuffer presentDrawable:Metal->Drawable];
	[CommandBuffer commit];

	[CommandBuffer waitUntilCompleted];

	R->CommandsFirst = Null;
	R->CommandsLast = Null;
}