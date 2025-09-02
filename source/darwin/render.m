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


#include <simd/simd.h>


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

	//ProcessAllPointsIntoComputeBuffers(R, v2i(ThreadGroupSize.width, ThreadGroupSize.height), v2i(Metal->Drawable.texture.width, Metal->Drawable.texture.height));

	v2i TileDimensions = v2i(ThreadGroupSize.width, ThreadGroupSize.height);
	int TileCount = TileDimensions.X*TileDimensions.Y;

	v2i WindowDimensions = v2i(Metal->Drawable.texture.width, Metal->Drawable.texture.height);

	typedef struct per_tile_data
	{
		int At;
		int Count;
		int Type;
	} per_tile_data;


	u64 CurrentBatchOffsetInBytes = 0;
	u64 VertexOffsetInBytes = 0;
	int PerTileOffset = 0;

	typedef struct circle
	{
		simd_float2 Position;
		simd_float4 Colour;
		float Radius;
	} circle;

	// NOTE(Oliver): Some kind of an upper limit
	int MAX_SHAPE_COUNT = 8192*2;
	circle *CirclesForThisDispatch = ArenaPushArray(Scratch.Arena, circle, MAX_SHAPE_COUNT);
	int TotalCircleCount = 0;
	int CircleCountSoFar = 0;

	typedef struct line
	{
		simd_float2 Start;
		simd_float2 End;
		float Thickness;
		simd_float4 Colour;
	} line;

	line *LinesForThisDispatch = ArenaPushArray(Scratch.Arena, line, MAX_SHAPE_COUNT);
	int TotalLineCount = 0;
	int LineCountSoFar = 0;

	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{
		u64 Stride = 0;
		switch(Command->Type)
		{
		case R_COMMAND_GLYPH:
		case R_COMMAND_TRIANGLE:
		{
			r_vertex_attributes Attributes = R_AttributesFromCommandType(Command->Type);
			Stride = MTLStrideFromAttributes(Attributes);
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
		} break;
		// NOTE(Oliver): This is for the compute SDF renderer
		case R_COMMAND_LINES:
		{
			int LineCount = Command->VerticesCount;
			for(int I = 0; I < LineCount; I++)
			{
				simd_float2 Start =
				{
					Command->Vertices[I].Start.X,
					Command->Vertices[I].Start.Y
				};
				simd_float2 End =
				{
					Command->Vertices[I].End.X,
					Command->Vertices[I].End.Y
				};

				simd_float4 C =
				{
					Command->Vertices[I].Colour.X,
					Command->Vertices[I].Colour.Y,
					Command->Vertices[I].Colour.Z,
					Command->Vertices[I].Colour.W
				};
				LinesForThisDispatch[LineCountSoFar + I].Start = Start;
				LinesForThisDispatch[LineCountSoFar + I].End = End;
				LinesForThisDispatch[LineCountSoFar + I].Thickness = Command->Vertices[I].Thickness;
				LinesForThisDispatch[LineCountSoFar + I].Colour = C;
			}
			
			LineCountSoFar += LineCount;
		}break;
		case R_COMMAND_POINTS:
		{			
			int CircleCount = Command->VerticesCount;
			for(int I = 0; I < CircleCount; I++)
			{
				simd_float2 P =
				{
					Command->Vertices[I].Position.X,
					Command->Vertices[I].Position.Y
				};
				simd_float4 C =
				{
					Command->Vertices[I].Colour.X,
					Command->Vertices[I].Colour.Y,
					Command->Vertices[I].Colour.Z,
					Command->Vertices[I].Colour.W
				};
				CirclesForThisDispatch[CircleCountSoFar + I].Position = P;
				CirclesForThisDispatch[CircleCountSoFar + I].Radius = Command->Vertices[I].PointSize;
				CirclesForThisDispatch[CircleCountSoFar + I].Colour = C;
			}
			
			CircleCountSoFar += CircleCount;
		}break;
		}
	
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
				per_tile_data *PerTileData = ArenaPushArrayZero(Scratch.Arena, per_tile_data, TileCount);

				for(int I = 0; I < CircleCountSoFar; I++)
				{
					simd_float2 P = CirclesForThisDispatch[I].Position;
					float R = CirclesForThisDispatch[I].Radius;
					v2f TileMin;
					TileMin.X = P.x - R;
					TileMin.Y = P.y - R;
					int TileMinX = (int)(floorf(TileMin.X / 32.0f));
					int TileMinY = (int)(floorf(TileMin.Y / 32.0f));

					v2f TileMax;
					TileMax.X = P.x + R;
					TileMax.Y = P.y + R;
					int TileMaxX = (int)(ceilf(TileMax.X / 32.0f));
					int TileMaxY = (int)(ceilf(TileMax.Y / 32.0f));

					TileMinX = Max(TileMinX, 0);
					TileMinY = Max(TileMinY, 0);

					TileMaxX = Min(TileMaxX, TileDimensions.X - 1);
					TileMaxY = Min(TileMaxY, TileDimensions.Y - 1);

					for(int X = TileMinX; X < TileMaxX; X++)
					{
						for(int Y = TileMinY; Y < TileMaxY; Y++)
						{
							int TileIndex = X + Y * TileDimensions.X;
							PerTileData[TileIndex].Count++;
						}
					}
				}

				int Count = 0;
				for(int I = 0; I < TileCount; I++)
				{
					PerTileData[I].At = Count;
					PerTileData[I].Type = 0; // Circle
					Count += PerTileData[I].Count;
				}

				int *TileOffsets = ArenaPushArrayZero(Scratch.Arena, int, TileCount);

				//TODO(Oliver): Find a better upper limit for this
				circle *CirclesInEachTile = ArenaPushArray(Scratch.Arena, circle, 50*TileCount);

				for(int I = 0; I < CircleCountSoFar; I++)
				{
					simd_float2 P = CirclesForThisDispatch[I].Position;
					float R = CirclesForThisDispatch[I].Radius;
					v2f TileMin;
					TileMin.X = P.x - R;
					TileMin.Y = P.y - R;
					int TileMinX = (int)(floorf(TileMin.X / 32.0f));
					int TileMinY = (int)(floorf(TileMin.Y / 32.0f));

					v2f TileMax;
					TileMax.X = P.x + R;
					TileMax.Y = P.y + R;
					int TileMaxX = (int)(ceilf(TileMax.X / 32.0f));
					int TileMaxY = (int)(ceilf(TileMax.Y / 32.0f));

					TileMinX = Max(TileMinX, 0);
					TileMinY = Max(TileMinY, 0);

					TileMaxX = Min(TileMaxX, TileDimensions.X - 1);
					TileMaxY = Min(TileMaxY, TileDimensions.Y - 1);

					for(int X = TileMinX; X < TileMaxX; X++)
					{
						for(int Y = TileMinY; Y < TileMaxY; Y++)
						{
							int TileIndex = X + Y * TileDimensions.X;
							int At = PerTileData[TileIndex].At;
							CirclesInEachTile[At + TileOffsets[TileIndex]] = CirclesForThisDispatch[I];
							TileOffsets[TileIndex] += 1;
						}
					}
				}

				MemoryCopy(Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT].contents + PerTileOffset*TileCount*sizeof(per_tile_data), PerTileData, TileCount*sizeof(per_tile_data));
				MemoryCopy(Metal->Data[R->Frame % FRAMES_IN_FLIGHT].contents + MegaBytes(0) + TotalCircleCount*sizeof(circle), CirclesInEachTile, Count*sizeof(circle));

				struct uniforms2
				{
					int NumberOfTilesInX;
					int NumberOfTilesInY;
					simd_float4 Scissor;
				} Uniforms = {
					.NumberOfTilesInX = ThreadGroupSize.width,
					.NumberOfTilesInY = ThreadGroupSize.height,
					.Scissor = {
							Command->Scissor.X, Command->Scissor.Y,
							Command->Scissor.Z, Command->Scissor.W
						}
				};

				MTLComputePassDescriptor *ComputePassDescriptor = [MTLComputePassDescriptor computePassDescriptor];
				ComputePassDescriptor.dispatchType = MTLDispatchTypeConcurrent;
				id<MTLComputeCommandEncoder> ComputeEncoder = [CommandBuffer computeCommandEncoderWithDescriptor:ComputePassDescriptor];
				[ComputeEncoder setComputePipelineState: Metal->ComputeTest.PipelineState];
				[ComputeEncoder setBytes:&Uniforms length:sizeof(Uniforms) atIndex:5];

				// These offsets are just temporary
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(0) + TotalCircleCount*sizeof(circle) atIndex:1];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(1) atIndex:2];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(2) atIndex:3];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(3) atIndex:4];
				[ComputeEncoder setBuffer:Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT] offset:PerTileOffset*TileCount*sizeof(per_tile_data) atIndex:0];
				[ComputeEncoder setTexture:Metal->BaseFrameBuffer atIndex:0];
				[ComputeEncoder setLabel:@"Render Points"];

				[ComputeEncoder dispatchThreads:MTLSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height, 1) threadsPerThreadgroup:TilesPerThreadgroup];

				[ComputeEncoder endEncoding];

				PerTileOffset++;
				TotalCircleCount += CircleCountSoFar;
				CircleCountSoFar = 0;
				
				//R_RenderPoints(R, CommandBuffer, Command->Scissor, VertexCount, VertexOffsetInBytes, UniformOffset*sizeof(uniforms));
			}
			else if(Command->Type == R_COMMAND_LINES)
			{
				per_tile_data *PerTileData = ArenaPushArrayZero(Scratch.Arena, per_tile_data, TileCount);

				for(int I = 0; I < LineCountSoFar; I++)
				{
					simd_float2 Start = LinesForThisDispatch[I].Start;
					simd_float2 End = LinesForThisDispatch[I].End;
					float Thickness = LinesForThisDispatch[I].Thickness;
					v2f TileMin;
					TileMin.X = Min(Start.x, End.x);
					TileMin.Y = Min(Start.y, End.y);
					int TileMinX = (int)(floorf(TileMin.X / 32.0f));
					int TileMinY = (int)(floorf(TileMin.Y / 32.0f));

					v2f TileMax;
					TileMax.X = Max(Start.x, End.x);
					TileMax.Y = Max(Start.y, End.y);
					int TileMaxX = (int)(ceilf(TileMax.X / 32.0f));
					int TileMaxY = (int)(ceilf(TileMax.Y / 32.0f));

					TileMinX = Max(TileMinX, 0);
					TileMinY = Max(TileMinY, 0);

					TileMaxX = Min(TileMaxX, TileDimensions.X - 1);
					TileMaxY = Min(TileMaxY, TileDimensions.Y - 1);

					for(int X = TileMinX; X < TileMaxX; X++)
					{
						for(int Y = TileMinY; Y < TileMaxY; Y++)
						{
							int TileIndex = X + Y * TileDimensions.X;
							PerTileData[TileIndex].Count++;
						}
					}
				}

				int CountSoFar = 0;
				for(int I = 0; I < TileCount; I++)
				{
					PerTileData[I].At = CountSoFar;
					// HACK(Oliver): Improve this
					PerTileData[I].Type = 1; // Line
					CountSoFar += PerTileData[I].Count;
				}

				int *TileOffsets = ArenaPushArrayZero(Scratch.Arena, int, TileCount);

				//TODO(Oliver): Find a better upper limit for this
				line *LinesInEachTile = ArenaPushArray(Scratch.Arena, line, 50*TileCount);

				for(int I = 0; I < LineCountSoFar; I++)
				{
					simd_float2 Start = LinesForThisDispatch[I].Start;
					simd_float2 End = LinesForThisDispatch[I].End;
					float Thickness = LinesForThisDispatch[I].Thickness;
					v2f TileMin;
					TileMin.X = Min(Start.x, End.x);
					TileMin.Y = Min(Start.y, End.y);
					int TileMinX = (int)(floorf(TileMin.X / 32.0f));
					int TileMinY = (int)(floorf(TileMin.Y / 32.0f));

					v2f TileMax;
					TileMax.X = Max(Start.x, End.x);
					TileMax.Y = Max(Start.y, End.y);
					int TileMaxX = (int)(ceilf(TileMax.X / 32.0f));
					int TileMaxY = (int)(ceilf(TileMax.Y / 32.0f));

					TileMinX = Max(TileMinX, 0);
					TileMinY = Max(TileMinY, 0);

					TileMaxX = Min(TileMaxX, TileDimensions.X - 1);
					TileMaxY = Min(TileMaxY, TileDimensions.Y - 1);

					for(int X = TileMinX; X < TileMaxX; X++)
					{
						for(int Y = TileMinY; Y < TileMaxY; Y++)
						{
							int TileIndex = X + Y * TileDimensions.X;
							int At = PerTileData[TileIndex].At;
							LinesInEachTile[At + TileOffsets[TileIndex]] = LinesForThisDispatch[I];
							TileOffsets[TileIndex] += 1;
						}
					}
				}

				MemoryCopy(Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT].contents + PerTileOffset*TileCount*sizeof(per_tile_data), PerTileData, TileCount*sizeof(per_tile_data));
				MemoryCopy(Metal->Data[R->Frame % FRAMES_IN_FLIGHT].contents + MegaBytes(1) + TotalLineCount*sizeof(line), LinesInEachTile, CountSoFar*sizeof(line));


				struct uniforms2
				{
					int NumberOfTilesInX;
					int NumberOfTilesInY;
					simd_float4 Scissor;
				} Uniforms = {
					.NumberOfTilesInX = ThreadGroupSize.width,
					.NumberOfTilesInY = ThreadGroupSize.height,
					.Scissor = {
							Command->Scissor.X, sapp_heightf() - Command->Scissor.Y,
							Command->Scissor.Z, Command->Scissor.W
						}
				};

				MTLComputePassDescriptor *ComputePassDescriptor = [MTLComputePassDescriptor computePassDescriptor];
				ComputePassDescriptor.dispatchType = MTLDispatchTypeConcurrent;
				id<MTLComputeCommandEncoder> ComputeEncoder = [CommandBuffer computeCommandEncoderWithDescriptor:ComputePassDescriptor];
				[ComputeEncoder setComputePipelineState: Metal->ComputeTest.PipelineState];
				[ComputeEncoder setBytes:&Uniforms length:sizeof(Uniforms) atIndex:5];

				// These offsets are just temporary
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(0) atIndex:1];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(1) + TotalLineCount*sizeof(line) atIndex:2];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(2) atIndex:3];
				[ComputeEncoder setBuffer:Metal->Data[R->Frame % FRAMES_IN_FLIGHT] offset:MegaBytes(3) atIndex:4];
				[ComputeEncoder setBuffer:Metal->PerTileData[R->Frame % FRAMES_IN_FLIGHT] offset:PerTileOffset*TileCount*sizeof(per_tile_data) atIndex:0];
				[ComputeEncoder setTexture:Metal->BaseFrameBuffer atIndex:0];
				[ComputeEncoder setLabel:@"Render Lines"];

				[ComputeEncoder dispatchThreads:MTLSizeMake(Metal->Drawable.texture.width, Metal->Drawable.texture.height, 1) threadsPerThreadgroup:TilesPerThreadgroup];

				[ComputeEncoder endEncoding];

				PerTileOffset++;
				TotalLineCount += LineCountSoFar;
				LineCountSoFar = 0;
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


	R_RenderFullscreenBloom(R, CommandBuffer);
	[CommandBuffer presentDrawable:Metal->Drawable];



	__block dispatch_semaphore_t BlockSemaphore = Metal->InFlightSemaphore;
	[CommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
	{
		dispatch_semaphore_signal(BlockSemaphore);
	}];

	[CommandBuffer commit];


	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;

	R->TransformCount = 0;
	R->ScissorCount = 0;

	ScratchEnd(Scratch);
}
