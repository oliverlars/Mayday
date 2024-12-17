#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <ModelIO/ModelIO.h>
#include <QuartzCore/QuartzCore.h>

#include "memory.h"
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
id<MTLTexture>             FontAtlasTexture;
id<MTLBuffer>              VertexBuffer;
id<MTLBuffer>              IndexBuffer;
id<MTLBuffer>              UniformBuffer;
id<MTLSamplerState>        SamplerState;

typedef struct uniforms
{
	m44f PerspectiveMatrix;
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

void R_PushCommand(r_context *R_Context, r_command Command)
{
	R_Context->TempCommand = Command;
}

// Dumb hack
void R_TextureFromBytes(r_context *R_Context, u8 *Bytes, s32 Width, s32 Height)
{
	MTLTextureDescriptor *TextureDescriptor = [MTLTextureDescriptor
											   texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
											   width:Width
											   height:Height
											   mipmapped:NO];
	TextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	FontAtlasTexture = [Device newTextureWithDescriptor:TextureDescriptor];
	MTLRegion region = MTLRegionMake2D(0, 0, Width, Height);
	[FontAtlasTexture replaceRegion:region mipmapLevel:0 withBytes:Bytes bytesPerRow:Width];
}

id<MTLTexture> MakeDepthTexture(r_context *R_Context, CGSize Size)
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

id<MTLTexture> ResizeDepthTexture(r_context *R_Context, CGSize Size)
{
	if(Size.width != [DepthTexture width] || Size.height != [DepthTexture height] || DepthTexture == nil)
	{
		DepthTexture = nil;

		DepthTexture = MakeDepthTexture(R_Context, Size);

		LogInfo(R_Context->Logger, "DepthTexture resized");
	}
	return DepthTexture;
}

void R_Init(r_context *R_Context)
{

	Device = (__bridge id<MTLDevice>)sapp_metal_get_device();

	CommandQueue = [Device newCommandQueue];

	VertexBuffer = [Device newBufferWithLength:4096*sizeof(gpu_vertex)
					options:MTLResourceStorageModeShared];

	UniformBuffer = [Device newBufferWithLength:sizeof(Uniforms)
					 options:MTLResourceStorageModeShared];

	NSError* Error = nil;
	NSString* Source = [NSString stringWithUTF8String:(char*)G_TriangleShader];
	id<MTLLibrary> Library = [Device newLibraryWithSource:Source options:nil error:&Error];
	id<MTLFunction> VertexFunction = [Library newFunctionWithName:@"VertexMain"];
	id<MTLFunction> FragmentFunction = [Library newFunctionWithName:@"FragmentMain"];

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
	SamplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
	SamplerDescriptor.lodMinClamp = 0.0;
	SamplerDescriptor.lodMaxClamp = FLT_MAX;
	SamplerDescriptor.normalizedCoordinates = YES;
	SamplerState = [Device newSamplerStateWithDescriptor:SamplerDescriptor];

	MTLDepthStencilDescriptor *DepthStencilDescriptor = [MTLDepthStencilDescriptor new];
	DepthStencilDescriptor.depthCompareFunction = MTLCompareFunctionLess;
	DepthStencilDescriptor.depthWriteEnabled = YES;
	DepthStencilState = [Device newDepthStencilStateWithDescriptor:DepthStencilDescriptor];

	PipelineState = [Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error:&Error];


}

f32 Angle = 0.0;

void R_Update(r_context *R_Context)
{
	id<CAMetalDrawable> Drawable = (__bridge id<CAMetalDrawable>)sapp_metal_get_current_drawable();
	MTLRenderPassDescriptor* PassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
	PassDesc.colorAttachments[0].texture = Drawable.texture;
	PassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
	PassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.05,0.05,0.05,1);

	PassDesc.depthAttachment.texture = ResizeDepthTexture(R_Context, CGSizeMake(Drawable.texture.width, Drawable.texture.height));
	PassDesc.depthAttachment.loadAction = MTLLoadActionClear;
	PassDesc.depthAttachment.clearDepth = 1.0;

	id<MTLCommandBuffer> CommandBuffer = [CommandQueue commandBuffer];

	Uniforms.ModelMatrix = M44FromXAngle(Angle);
	Angle += 0.01;
	Uniforms.ModelMatrix = MulM44FM44F(Uniforms.ModelMatrix, M44FTranslate(v3f(0, 2 * sinf(Angle), 0)));
	
	Uniforms.PerspectiveMatrix = M44FPerspective((f32)Drawable.texture.width/(f32)Drawable.texture.height, 100.0, 0.1, 10000.0);
	Uniforms.ViewMatrix = M44FLookAt(v3f(0, 0, 10), v3f(0, 0, 0), v3f(0, 1, 0));

	MemoryCopy(UniformBuffer.contents, &Uniforms, sizeof(Uniforms));

	// TODO(Oliver): sMake this a proper construct
	u64 ArenaResetTo = R_Context->Arena->At;

	gpu_vertex *GPUVertices = (gpu_vertex*)ArenaPush(R_Context->Arena, sizeof(gpu_vertex)*R_Context->TempCommand.VerticesCount);
	for(int V = 0; V < R_Context->TempCommand.VerticesCount; V++)
	{
		v4f P = R_Context->TempCommand.Vertices[V].Position;
		v2f UV = R_Context->TempCommand.Vertices[V].UV;
		GPUVertices[V].Position = P;
		GPUVertices[V].Colour = v4f(0, 1, 0, 1);
		GPUVertices[V].UV = v4f(UV.X, UV.Y, 0, 0);
	}
	MemoryCopy(VertexBuffer.contents, GPUVertices, sizeof(gpu_vertex)*R_Context->TempCommand.VerticesCount);

	R_Context->Arena->At = ArenaResetTo;

	id<MTLRenderCommandEncoder> Encoder = [CommandBuffer renderCommandEncoderWithDescriptor:PassDesc];
	[Encoder setRenderPipelineState:PipelineState];
	[Encoder setFrontFacingWinding:MTLWindingClockwise];
	[Encoder setCullMode:(MTLCullModeNone)];
	[Encoder setVertexBuffer:VertexBuffer offset:0 atIndex:0];
	[Encoder setVertexBuffer:UniformBuffer offset:0 atIndex:1];
	[Encoder setDepthStencilState:DepthStencilState];
	[Encoder setFragmentTexture:FontAtlasTexture atIndex:0];
	[Encoder setFragmentSamplerState:SamplerState atIndex:0];
	// [Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:36
	// 														indexType:MTLIndexTypeUInt32
	// 														indexBuffer:IndexBuffer
	// 														indexBufferOffset:0];
	[Encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:R_Context->TempCommand.VerticesCount];
			 
	[Encoder endEncoding];

	[CommandBuffer presentDrawable:Drawable];
	[CommandBuffer commit];
}