#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>
#include <simd/simd.h>

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

#undef I

typedef struct uniforms
{
	m44f PVMMatrix;
} uniforms;

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

#define _METAL_TYPES_v4f Float4
#define _METAL_TYPES_v2f Float2
#define _METAL_TYPES_f32 Float
#define _METAL_TYPES_u32 UInt
#define MetalTypes(T) _METAL_TYPES_##T

#define _METAL_SIMD_TYPES_v4f simd_float4
#define _METAL_SIMD_TYPES_v2f simd_float2
#define _METAL_SIMD_TYPES_f32 f32
#define _METAL_SIMD_TYPES_u32 u32
#define MetalSimdTypes(T) _METAL_SIMD_TYPES_##T


int MTLVertexAlignmentFromAttribute(r_vertex_attributes Attribute)
{
	// NOTE(Oliver): We are assuming only one flag was passed in
	int Index = CTZ_U64(Attribute);
	#define X(T, NAME, CAPS_NAME, INDEX) alignof(MetalSimdTypes(T)),
	int LUT[] = 
	{
		VERTEX_ATTRIBUTE_FIELDS
	};
	#undef X

	return LUT[Index];
}

int MTLVertexSizeFromAttribute(r_vertex_attributes Attribute)
{
	// NOTE(Oliver): We are assuming only one flag was passed in
	int Index = CTZ_U64(Attribute);
	#define X(T, NAME, CAPS_NAME, INDEX) sizeof(MetalSimdTypes(T)),
	int LUT[] = 
	{
		VERTEX_ATTRIBUTE_FIELDS
	};
	#undef X

	return LUT[Index];
}

MTLVertexFormat MTLVertexFormatFromAttribute(r_vertex_attributes Attribute)
{
	// NOTE(Oliver): We are assuming only one flag was passed in
	int Index = CTZ_U64(Attribute);
	#define X(T, NAME, CAPS_NAME, INDEX) Concat(MTLVertexFormat, MetalTypes(T)),
	MTLVertexFormat LUT[] = 
	{
		VERTEX_ATTRIBUTE_FIELDS
	};
	#undef X

	return LUT[Index];
}

int MTLStrideFromAttributes(r_vertex_attributes Attributes)
{
	s64 Stride = 0;
	for(int AttributeIndex = 0;
		AttributeIndex < ArrayCount(G_VertexAttributeSizes);
		AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			int Size = MTLVertexSizeFromAttribute(G_VertexAttributes[AttributeIndex]);
			int Alignment = MTLVertexAlignmentFromAttribute(G_VertexAttributes[AttributeIndex]);
			Stride += Align(Size, Alignment);
		}
	}
	return Align(Stride, 16);
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

metal_compute_shader R_ComputeShader(render_context *R)
{
	metal_context *Metal = (metal_context*)R->Backend;
	metal_compute_shader Shader = {0};

	NSError* Error = nil;
	NSString* Source = [[NSString alloc] initWithBytes:(const void*)G_ComputeShader.Data
									length:G_ComputeShader.Length
									encoding:NSUTF8StringEncoding];
	Shader.Library = [Metal->Device newLibraryWithSource:Source options:nil error:&Error];
	if(!Shader.Library)
	{
		LogError((char*)[Error.localizedDescription
						 cStringUsingEncoding:NSUTF8StringEncoding]);
	}

	id<MTLFunction> KernelFunction   = [Shader.Library newFunctionWithName:@"ComputeMain"];

	Shader.PipelineState = [Metal->Device newComputePipelineStateWithFunction:KernelFunction error:&Error];

	return Shader;
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
	r_vertex_attributes Attributes = R_AttributesFromCommandType(R_COMMAND_GLYPH);
	
	int Index = 0;
	int Offset = 0;
	int MaxAlignment = 0; 
	for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			int ThisAlignment = MTLVertexAlignmentFromAttribute(G_VertexAttributes[AttributeIndex]);
			MaxAlignment = Max(MaxAlignment, ThisAlignment);

			Offset = Align(Offset, ThisAlignment);
			VertexDescriptor.attributes[Index].format = MTLVertexFormatFromAttribute(G_VertexAttributes[AttributeIndex]);
			VertexDescriptor.attributes[Index].offset = Offset;
			VertexDescriptor.attributes[Index].bufferIndex = 0;
			Index += 1;

			int Size = MTLVertexSizeFromAttribute(G_VertexAttributes[AttributeIndex]);
			Offset += Size;
		}
	}
	int Stride = Align(Offset, MaxAlignment);
	VertexDescriptor.layouts[0].stride = Stride;

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
	r_vertex_attributes Attributes = R_AttributesFromCommandType(R_COMMAND_TRIANGLE);
	
	int Index = 0;
	int Offset = 0;
	for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			VertexDescriptor.attributes[Index].format = MTLVertexFormatFromAttribute(G_VertexAttributes[AttributeIndex]);
			VertexDescriptor.attributes[Index].offset = Offset;
			VertexDescriptor.attributes[Index].bufferIndex = 0;
			Index += 1;
			Offset += MTLVertexAlignmentFromAttribute(G_VertexAttributeSizes[AttributeIndex]);
		}
	}
	VertexDescriptor.layouts[0].stride = Offset;

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
	r_vertex_attributes Attributes = R_AttributesFromCommandType(R_COMMAND_POINTS);
	
	int Index = 0;
	int Offset = 0;
	for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			VertexDescriptor.attributes[Index].format = MTLVertexFormatFromAttribute(G_VertexAttributes[AttributeIndex]);
			VertexDescriptor.attributes[Index].offset = Offset;
			VertexDescriptor.attributes[Index].bufferIndex = 0;
			Index += 1;
			int Size = MTLVertexSizeFromAttribute(G_VertexAttributes[AttributeIndex]);
			int Alignment = MTLVertexAlignmentFromAttribute(G_VertexAttributes[AttributeIndex]);
			Offset += Size;
			Offset = Align(Offset, Alignment);
		}
	}
	int Stride = Align(Offset, 16);
	VertexDescriptor.layouts[0].stride = Stride;

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
	r_vertex_attributes Attributes = R_AttributesFromCommandType(R_COMMAND_LINES);
	int Index = 0;
	int Offset = 0;
	for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			VertexDescriptor.attributes[Index].format = MTLVertexFormatFromAttribute(G_VertexAttributes[AttributeIndex]);
			VertexDescriptor.attributes[Index].offset = Offset;
			VertexDescriptor.attributes[Index].bufferIndex = 0;
			Index += 1;
			Offset += MTLVertexAlignmentFromAttribute(G_VertexAttributeSizes[AttributeIndex]);
		}
	}
	VertexDescriptor.layouts[0].stride = Offset;

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
	r_vertex_attributes Attributes = R_AttributesFromCommandType(R_COMMAND_BEZIER);
	
	int Index = 0;
	int Offset = 0;
	for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
	{
		if(Attributes & G_VertexAttributes[AttributeIndex])
		{
			VertexDescriptor.attributes[Index].format = MTLVertexFormatFromAttribute(G_VertexAttributes[AttributeIndex]);
			VertexDescriptor.attributes[Index].offset = Offset;
			VertexDescriptor.attributes[Index].bufferIndex = 0;
			Index += 1;
			Offset += MTLVertexAlignmentFromAttribute(G_VertexAttributeSizes[AttributeIndex]);
		}
	}
	VertexDescriptor.layouts[0].stride = Offset;

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

		Metal->CurveBuffer[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(128)
			options:MTLResourceStorageModeShared];


		[Metal->PerTileData[ResourceIndex] setLabel:@"Tile Data"];
		Metal->Data[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(128)
			options:MTLResourceStorageModeShared];


		Metal->PerTileData[ResourceIndex] = [Metal->Device newBufferWithLength:MegaBytes(128)
			options:MTLResourceStorageModeShared];
		[Metal->PerTileData[ResourceIndex] setLabel:@"Per Tile Data"];
	}

	{
		MTLTextureDescriptor *Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
														width:Size.X
														height:Size.Y mipmapped:NO];
		Desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
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
	Metal->ComputeTest = R_ComputeShader(R);

	Metal->InFlightSemaphore = dispatch_semaphore_create(FRAMES_IN_FLIGHT);
}


void ProcessAllPointsIntoComputeBuffers(render_context *R, v2i TileDimensions, v2i WindowDimensions)
{
	metal_context *Metal = R->Backend;

	typedef struct per_tile_data
	{
		int At;
		int Count;
	} per_tile_data;
	typedef struct data
	{
		v2f Position;
		f32 Radius;
	} data;

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	per_tile_data *PerTileData = ArenaPushArrayZero(Scratch.Arena, per_tile_data, TileDimensions.X*TileDimensions.Y);
	
	// The most amount per tile we can have is 20 and that's a super edge case
	// that i probably should never handle. That ends up being:
	// NumberOfTiles * NumberOfShapes
	data *Data = ArenaPushArrayZero(Scratch.Arena, data, 20*TileDimensions.X*TileDimensions.Y);
	
	random32_state State = XorShiftInit();

	data *Circles = ArenaPushArrayZero(Scratch.Arena, data, 20);
	for(int I = 0; I < 20; I++)
	{
		f32 X = RandomUnilateral(&State) * WindowDimensions.X;
		f32 Y = RandomUnilateral(&State) * WindowDimensions.Y;
		Circles[I].Position = v2f(X, Y);
		Circles[I].Radius = RandomUnilateral(&State) * 50;
	}


	int TotalCount = 0;
	for(int I = 0; I < TileDimensions.X*TileDimensions.Y; I++)
	{
		int TileX = I % TileDimensions.X;
		int TileY = I / TileDimensions.Y;

		v2i Corners[4] = 
		{
			v2i(TileX*32, TileY*32),
			v2i(TileX*32 + 32, TileY*32),
			v2i(TileX*32 + 32, TileY*32 + 32),
			v2i(TileX*32, TileY*32 + 32),
		};
		PerTileData[I].At = TotalCount;
		for(int J = 0; J < 20; J++)
		{
			for(int C = 0; C < 4; C++)
			{
				v2i DistanceToCircle = {Circles[J].Position.X, Circles[J].Position.Y};
				DistanceToCircle.X -= Corners[C].X;
				DistanceToCircle.Y -= Corners[C].Y;
				if(sqrtf(DistanceToCircle.X*DistanceToCircle.X + DistanceToCircle.Y*DistanceToCircle.Y) < Circles[J].Radius)
				{
					PerTileData[I].Count++;
					Data[TotalCount] = Circles[J];
					TotalCount++;
					break;
				}
			}
		}
	}

	MemoryCopy(Metal->Data[R->Frame % FRAMES_IN_FLIGHT].contents, Data, TotalCount*sizeof(data));
	MemoryCopy(Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT].contents, PerTileData, TileDimensions.X*TileDimensions.Y*sizeof(per_tile_data));



	ScratchEnd(Scratch);
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
	// This seems to fix blending for points? Not sure why it can't just be 
	// MTLCompareFunctionLess
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLessEqual;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	id<MTLDepthStencilState> DepthStencilState = [Metal->Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setLabel:@"Render Points"];
	[Encoder setRenderPipelineState:Metal->PointShader.PipelineState];
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

void R_RenderLines(render_context *R, id<MTLCommandBuffer> CommandBuffer, v4i Scissor, int VertexCount, int VertexOffset, int UniformOffset)
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
	
	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setLabel:@"Render Lines"];
	[Encoder setRenderPipelineState:Metal->LineShader.PipelineState];
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

	[Encoder setLabel:@"Render Beziers"];
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
	[Encoder setLabel:@"Render Triangles"];
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

	[Encoder setLabel:@"Render Glyphs"];
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
	[Encoder setLabel:@"Render Bloom"];
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


	int ResourceIndex = R->Frame % FRAMES_IN_FLIGHT;

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	int UniformOffset = 0;

	NSUInteger TileWidth = 32;
	NSUInteger TileHeight = 32;

	MTLSize TilesPerThreadgroup = MTLSizeMake(TileWidth, TileHeight, 1);
	MTLSize ThreadsPerGrid = MTLSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height, 1);
	MTLSize ThreadGroupSize = MTLSizeMake(
		(ThreadsPerGrid.width  + TileWidth  - 1) / TileWidth,
		(ThreadsPerGrid.height + TileHeight - 1) / TileHeight,
		1
	);

	ProcessAllPointsIntoComputeBuffers(R, v2i(ThreadGroupSize.width, ThreadGroupSize.height), v2i(Metal->Drawable.texture.width, Metal->Drawable.texture.height));

	u64 CurrentBatchOffsetInBytes = 0;
	u64 VertexOffsetInBytes = 0;
	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{
		r_vertex_attributes Attributes = R_AttributesFromCommandType(Command->Type);
		u64 Stride = MTLStrideFromAttributes(Attributes);
		u8 *GPUVertices = (u8*)ArenaPushArrayZero(Scratch.Arena, u8, Command->VerticesCount*Stride);
		int MaxAlignment = 0;
		for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
		{
			if(Attributes & G_VertexAttributes[AttributeIndex])
			{
				MaxAlignment = Max(MaxAlignment, MTLVertexAlignmentFromAttribute(G_VertexAttributes[AttributeIndex]));
			}
		}

		u64 Offset = 0;
		for(int VertexIndex = 0; VertexIndex < Command->VerticesCount; VertexIndex++)
		{
			r_vertex *V = Command->Vertices + VertexIndex;
			for(int AttributeIndex = 0; AttributeIndex < ArrayCount(G_VertexAttributes); AttributeIndex++)
			{
				if(Attributes & G_VertexAttributes[AttributeIndex])
				{
					int ThisAlignment = MTLVertexAlignmentFromAttribute(G_VertexAttributes[AttributeIndex]);
					Offset = Align(Offset, ThisAlignment);
					u8 *AttributeOffset = (u8*)V + G_VertexAttributeOffsets[AttributeIndex];
					
					int AttributeSize = MTLVertexSizeFromAttribute(G_VertexAttributes[AttributeIndex]);
					MemoryCopy(GPUVertices + Offset, AttributeOffset, AttributeSize);
					Offset += AttributeSize;
				}
			}
			// Pad to full stride after each vertex
			Offset = Align(Offset, MaxAlignment);
		}

		MemoryCopy(Metal->VertexBuffers[ResourceIndex].contents + CurrentBatchOffsetInBytes + VertexOffsetInBytes,
					GPUVertices, Offset);
		CurrentBatchOffsetInBytes += Offset;

		if((Command->Next && Command->Next->Type != Command->Type) ||
		   (Command->Next && !MemoryEqual(&Command->Transform, &Command->Next->Transform, sizeof(m44f))) ||
		   (!Command->Next))
		{
			uniforms Uniforms = { Command->Transform };
			MemoryCopy(Metal->UniformBuffers[R->Frame % FRAMES_IN_FLIGHT].contents + UniformOffset*sizeof(uniforms), &Uniforms, sizeof(Uniforms));

			u64 VertexCount = CurrentBatchOffsetInBytes/Stride;
			if(Command->Type == R_COMMAND_GLYPH)
			{
				R_RenderGlyphs(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
			}
			else if(Command->Type == R_COMMAND_TRIANGLE)
			{
				R_RenderTriangles(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
			}
			else if(Command->Type == R_COMMAND_POINTS)
			{
				R_RenderPoints(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
			}
			else if(Command->Type == R_COMMAND_LINES)
			{
				R_RenderLines(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
			}
			else if(Command->Type == R_COMMAND_BEZIER)
			{
				R_RenderBeziers(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));;
			}

			VertexOffsetInBytes += CurrentBatchOffsetInBytes;
			CurrentBatchOffsetInBytes = 0;

			UniformOffset += 1;
		}
	}

	MTLComputePassDescriptor *ComputePassDescriptor = [MTLComputePassDescriptor computePassDescriptor];
	ComputePassDescriptor.dispatchType = MTLDispatchTypeConcurrent;
	id<MTLComputeCommandEncoder> ComputeEncoder = [CommandBuffer computeCommandEncoderWithDescriptor:ComputePassDescriptor];
	[ComputeEncoder setComputePipelineState: Metal->ComputeTest.PipelineState];
	[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:1];
	[ComputeEncoder setBuffer:Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT] offset:0 atIndex:0];
	[ComputeEncoder setTexture:Metal->BaseFrameBuffer atIndex:0];
	[ComputeEncoder dispatchThreadgroups:ThreadGroupSize threadsPerThreadgroup:TilesPerThreadgroup];

	[ComputeEncoder endEncoding];

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
