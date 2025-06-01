#include <assert.h>
#include <d3d11.h>

#include "memory.h"
#include "misc.h"
#include "sokol_app.h"

#include "os.h"
#include "maths.h"
#include "render.h"
#include "shaders.h"
#include "log.h"

#include "win32/d3d11_render.h"

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
	v3f Padding;
}
gpu_vertex;

void R_TextureFromBytes(render_context *R, u8 *Bytes, s32 Width, s32 Height)
{
	d3d11_context *D3D11 = R->Backend;
	if(!D3D11->FontAtlasTexture)
	{
		D3D11_TEXTURE2D_DESC TextureDescription = {0};
		TextureDescription.Width              = (UINT)Width;
		TextureDescription.Height             = (UINT)Height;
		TextureDescription.MipLevels          = 1;
		TextureDescription.ArraySize          = 1;
		TextureDescription.Format             = DXGI_FORMAT_R8_UNORM;
		TextureDescription.SampleDesc.Count   = 1;
		TextureDescription.SampleDesc.Quality = 0;
		TextureDescription.Usage              = D3D11_USAGE_DEFAULT;
		TextureDescription.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
		TextureDescription.CPUAccessFlags     = 0;
		TextureDescription.MiscFlags          = 0;

		D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&TextureDescription, NULL,
			&D3D11->FontAtlasTexture);
		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->FontAtlasTexture,
			Null,
			&D3D11->FontAtlasTextureView
		);
	}

	D3D11->DeviceContext->lpVtbl->UpdateSubresource(
		D3D11->DeviceContext,
		(ID3D11Resource*)D3D11->FontAtlasTexture,
		0,
		NULL,
		Bytes,
		Width * sizeof(u8),
		0
	);

}


void R_Init(render_context *R, m_arena *Arena, v2i Size)
{
	d3d11_context *D3D11 = ArenaPushArrayZero(Arena, d3d11_context, 1);
	R->Backend = D3D11;

	D3D11->Device = (ID3D11Device*)sapp_d3d11_get_device();
	D3D11->DeviceContext = (ID3D11DeviceContext*)sapp_d3d11_get_device_context();

	for(int ResourceIndex = 0; ResourceIndex < FRAMES_IN_FLIGHT; ResourceIndex++)
	{
		D3D11_BUFFER_DESC UniformBufferDescription = {0};
		UniformBufferDescription.ByteWidth         = MegaBytes(16);
		UniformBufferDescription.Usage             = D3D11_USAGE_DYNAMIC;
		UniformBufferDescription.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
		UniformBufferDescription.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&UniformBufferDescription,
											NULL,
											&D3D11->UniformBuffers[ResourceIndex]);

		D3D11_BUFFER_DESC TriangleVertexBufferDescription = {0};
		TriangleVertexBufferDescription.ByteWidth = MegaBytes(16);
		TriangleVertexBufferDescription.Usage     = D3D11_USAGE_DYNAMIC;
		TriangleVertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		TriangleVertexBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&TriangleVertexBufferDescription,
											NULL,
											&D3D11->VertexBuffers[ResourceIndex]);

		D3D11_BUFFER_DESC GlyphVertexBufferDescription = {0};
		GlyphVertexBufferDescription.ByteWidth = MegaBytes(16);
		GlyphVertexBufferDescription.Usage     = D3D11_USAGE_DYNAMIC;
		GlyphVertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		GlyphVertexBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&GlyphVertexBufferDescription,
											NULL,
											&D3D11->VertexBuffers[ResourceIndex]);
	}


	// TRIANGLE SHADER
	{
		ID3DBlob *TriangleVertexShaderObject;
		D3DCompile(G_TriangleShader, sizeof(G_TriangleShader),
				Null, Null, Null,
				"VertexMain",
				"vs_5_0",
				0, 0,
				&TriangleVertexShaderObject, Null);
		
		D3D11->Device->lpVtbl->CreateVertexShader(
			D3D11->Device,
			TriangleVertexShaderObject->lpVtbl->GetBufferPointer(TriangleVertexShaderObject),
			TriangleVertexShaderObject->lpVtbl->GetBufferSize(TriangleVertexShaderObject),
			Null, &D3D11->TriangleVertexShader
		);

		D3D11_INPUT_ELEMENT_DESC VertexAttributes[] =
		{
			{ "POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11->Device->lpVtbl->CreateInputLayout(
			D3D11->Device,
			VertexAttributes,
			ARRAYSIZE(VertexAttributes),
			TriangleVertexShaderObject->lpVtbl->GetBufferPointer(TriangleVertexShaderObject),
			TriangleVertexShaderObject->lpVtbl->GetBufferSize(TriangleVertexShaderObject),
			&D3D11->TriangleInputLayout);

		ID3DBlob *TrianglePixelShaderObject;
		D3DCompile(G_TriangleShader, sizeof(G_TriangleShader),
				Null, Null, Null,
				"PixelMain",
				"ps_5_0",
				0, 0,
				&TrianglePixelShaderObject, Null);

		D3D11->Device->lpVtbl->CreatePixelShader(
			D3D11->Device,
			TrianglePixelShaderObject->lpVtbl->GetBufferPointer(TrianglePixelShaderObject),
			TrianglePixelShaderObject->lpVtbl->GetBufferSize(TrianglePixelShaderObject),
			Null, &D3D11->TrianglePixelShader
		);
	}

	// GLYPH SHADER
	{
		ID3DBlob *GlyphVertexShaderObject;
		D3DCompile(G_GlyphShader, sizeof(G_GlyphShader),
				Null, Null, Null,
				"VertexMain",
				"vs_5_0",
				0, 0,
				&GlyphVertexShaderObject, Null);
		
		D3D11->Device->lpVtbl->CreateVertexShader(
			D3D11->Device,
			GlyphVertexShaderObject->lpVtbl->GetBufferPointer(GlyphVertexShaderObject),
			GlyphVertexShaderObject->lpVtbl->GetBufferSize(GlyphVertexShaderObject),
			Null, &D3D11->GlyphVertexShader
		);


		// TODO(Oliver): Why does this even have the point size on here
		D3D11_INPUT_ELEMENT_DESC VertexAttributes[] =
		{
			{ "POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,                            0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "PSIZE", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11->Device->lpVtbl->CreateInputLayout(
			D3D11->Device,
			VertexAttributes,
			ARRAYSIZE(VertexAttributes),
			GlyphVertexShaderObject->lpVtbl->GetBufferPointer(GlyphVertexShaderObject),
			GlyphVertexShaderObject->lpVtbl->GetBufferSize(GlyphVertexShaderObject),
			&D3D11->GlyphInputLayout);

		ID3DBlob *GlyphPixelShaderObject;
		D3DCompile(G_GlyphShader, sizeof(G_GlyphShader),
				Null, Null, Null,
				"PixelMain",
				"ps_5_0",
				0, 0,
				&GlyphPixelShaderObject, Null);

		D3D11->Device->lpVtbl->CreatePixelShader(
			D3D11->Device,
			GlyphPixelShaderObject->lpVtbl->GetBufferPointer(GlyphPixelShaderObject),
			GlyphPixelShaderObject->lpVtbl->GetBufferSize(GlyphPixelShaderObject),
			Null, &D3D11->GlyphPixelShader
		);
	}

	D3D11_RASTERIZER_DESC RasterizerStateDescription = {};
	RasterizerStateDescription.FillMode = D3D11_FILL_SOLID;
	// HACK(Oliver): Disabling culling is only because I
	// fucked up the winding order...
	RasterizerStateDescription.CullMode = D3D11_CULL_NONE;
	RasterizerStateDescription.FrontCounterClockwise = True;

    D3D11->Device->lpVtbl->CreateRasterizerState(
		D3D11->Device,
		&RasterizerStateDescription,
		&D3D11->RasterizerState
	);

	D3D11_DEPTH_STENCIL_DESC DepthStencilDescription = {};
    DepthStencilDescription.DepthEnable    = True;
    DepthStencilDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthStencilDescription.DepthFunc      = D3D11_COMPARISON_LESS;

    D3D11->Device->lpVtbl->CreateDepthStencilState(
		D3D11->Device,
		&DepthStencilDescription,
		&D3D11->DepthStencilState
	);

	D3D11_SAMPLER_DESC SamplerDescription = {};
	SamplerDescription.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SamplerDescription.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDescription.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDescription.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDescription.ComparisonFunc = D3D11_COMPARISON_NEVER;


	D3D11->Device->lpVtbl->CreateSamplerState(
		D3D11->Device,
		&SamplerDescription,
		&D3D11->SamplerState
	);

	D3D11_BLEND_DESC BlendDescription = {0};
	BlendDescription.RenderTarget[0].BlendEnable = TRUE;
	BlendDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	D3D11->Device->lpVtbl->CreateBlendState(
		D3D11->Device,
		&BlendDescription,
		&D3D11->BlendState
	);

}

ID3D11Texture2D *ResizeTexture(render_context *R, ID3D11Texture2D *Texture, v2i Size)
{
	d3d11_context *D3D11 = R->Backend;
	ID3D11Texture2D *Result = Texture;
	D3D11_TEXTURE2D_DESC TextureDescription;
	Texture->lpVtbl->GetDesc(Texture, &TextureDescription);
	if(Size.X != TextureDescription.Width || Size.Y != TextureDescription.Height || Texture == Null)
	{
		Texture->lpVtbl->Release(Texture);

		TextureDescription.Width = Size.X;
		TextureDescription.Height = Size.Y;


		D3D11->Device->lpVtbl->CreateTexture2D(D3D11->Device,
										&TextureDescription,
										Null,
										&Result);

		LogInfo("Texture resized");
	}

	return Result;
}

void R_Update(render_context *R, m_arena *FrameArena)
{
	d3d11_context *D3D11 = R->Backend;
	IDXGISwapChain *SwapChain = (IDXGISwapChain*)sapp_d3d11_get_swap_chain();


	ID3D11Texture2D *FrameBuffer;
	SwapChain->lpVtbl->GetBuffer(SwapChain, 0, &IID_ID3D11Texture2D, (void**)&FrameBuffer);
	D3D11_TEXTURE2D_DESC FrameBufferDescription;	
	FrameBuffer->lpVtbl->GetDesc(FrameBuffer, &FrameBufferDescription);
	v2i Dimensions = v2i(FrameBufferDescription.Width, FrameBufferDescription.Height);

	D3D11_VIEWPORT viewport =
	{
		0.0f, 0.0f, 
		(f32)FrameBufferDescription.Width,
		(f32)FrameBufferDescription.Height,
		0.0f, 1.0f
	};
	

	if(D3D11->DepthTexture == Null)
	{
		D3D11_TEXTURE2D_DESC DepthTextureDescription = FrameBufferDescription;
		DepthTextureDescription.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthTextureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		D3D11->Device->lpVtbl->CreateTexture2D(D3D11->Device,
											   &DepthTextureDescription,
											   Null,
											   &D3D11->DepthTexture);

		D3D11->Device->lpVtbl->CreateDepthStencilView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->DepthTexture,
			Null, &D3D11->DepthStencilView);
	}
	else
	{
		ResizeTexture(R, D3D11->DepthTexture, Dimensions);
	}

	f32 ClearColour[4] = {1.0, 0.0, 0.0, 1.0};
	ID3D11RenderTargetView *RenderTargetView = (ID3D11RenderTargetView*)sapp_d3d11_get_render_view();
	D3D11->DeviceContext->lpVtbl->ClearRenderTargetView(
		D3D11->DeviceContext,
		RenderTargetView,
		ClearColour
	);

	D3D11->DeviceContext->lpVtbl->ClearDepthStencilView(
		D3D11->DeviceContext,
		D3D11->DepthStencilView,
		D3D11_CLEAR_DEPTH,
		1.0f, 0
	);


	m_scratch_arena Scratch = ScratchBegin(0, 0);

	
	int TotalVertices = 0;
	int VertexOffset = 0;

	int ResourceIndex = R->Frame % FRAMES_IN_FLIGHT;

	int UniformCount = 0;
	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{
		if(0){}
		else if(Command->Type == R_COMMAND_TRIANGLE || Command->Type == R_COMMAND_GLYPH)
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
				GPUVertices[V].UV = v4f(UV.X, UV.Y, 0, 1);
			}

			D3D11_MAPPED_SUBRESOURCE TriangleVertexBufferResource;

			D3D11->DeviceContext->lpVtbl->Map(
				D3D11->DeviceContext,
				(ID3D11Resource*)D3D11->VertexBuffers[ResourceIndex],
				0,
				D3D11_MAP_WRITE_DISCARD, 0, &TriangleVertexBufferResource
			);
			gpu_vertex* Vertices = (gpu_vertex*)TriangleVertexBufferResource.pData;

			MemoryCopy(
				Vertices + VertexOffset + TotalVertices,
				GPUVertices,
				sizeof(gpu_vertex)*Command->VerticesCount
			);
			D3D11->DeviceContext->lpVtbl->Unmap(
				D3D11->DeviceContext,
				(ID3D11Resource*)D3D11->VertexBuffers[ResourceIndex],
				0
			);

			TotalVertices += Command->VerticesCount;
		}
		else if(Command->Type == R_COMMAND_LINES)
		{

		}

		if((Command->Next && Command->Next->Type != Command->Type) ||
		   (Command->Next && !MemoryEqual(&Command->Transform, &Command->Next->Transform, sizeof(m44f))) ||
		   (!Command->Next))
		{
			D3D11_MAPPED_SUBRESOURCE UniformBufferResourse;
			D3D11->DeviceContext->lpVtbl->Map(
				D3D11->DeviceContext,
				(ID3D11Resource*)D3D11->UniformBuffers[ResourceIndex],
				0,
				D3D11_MAP_WRITE_DISCARD,
				0,
				&UniformBufferResourse); // update constant buffer every frame
			{
				uniforms* Uniforms = (uniforms*)UniformBufferResourse.pData;

				Uniforms->PVMMatrix = Command->Transform;
			}
			D3D11->DeviceContext->lpVtbl->Unmap(
				D3D11->DeviceContext,
				(ID3D11Resource*)D3D11->UniformBuffers[ResourceIndex],
				0
			);

			if(Command->Type == R_COMMAND_TRIANGLE)
			{
				D3D11->DeviceContext->lpVtbl->IASetPrimitiveTopology(D3D11->DeviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				D3D11->DeviceContext->lpVtbl->IASetInputLayout(D3D11->DeviceContext, D3D11->TriangleInputLayout);
				u32 Stride = sizeof(gpu_vertex);
				u32 Offset = 0;
				D3D11->DeviceContext->lpVtbl->IASetVertexBuffers(D3D11->DeviceContext, 0, 1, &D3D11->VertexBuffers[ResourceIndex], &Stride, &Offset);
				D3D11->DeviceContext->lpVtbl->VSSetShader(
					D3D11->DeviceContext,
					D3D11->TriangleVertexShader,
					Null, 
					0
				);
				D3D11->DeviceContext->lpVtbl->VSSetConstantBuffers(
					D3D11->DeviceContext,
					0, 1,
					&D3D11->UniformBuffers[ResourceIndex]
				);
				D3D11->DeviceContext->lpVtbl->RSSetViewports(
					D3D11->DeviceContext,
					1, &viewport
				);

				D3D11_RECT ScissorRect =
				{
					Command->Scissor.X,
					Command->Scissor.Y,
					Command->Scissor.X + Command->Scissor.Z,
					Command->Scissor.Y + Command->Scissor.W,
				};
				D3D11->DeviceContext->lpVtbl->RSSetScissorRects(
					D3D11->DeviceContext,
					1,
					&ScissorRect
				);
				D3D11->DeviceContext->lpVtbl->RSSetState(
					D3D11->DeviceContext,
					D3D11->RasterizerState
				);
				D3D11->DeviceContext->lpVtbl->PSSetShader(
					D3D11->DeviceContext,
					D3D11->TrianglePixelShader,
					Null, 0
				);
				D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
					D3D11->DeviceContext, 
					1, &RenderTargetView, D3D11->DepthStencilView
				);
				D3D11->DeviceContext->lpVtbl->OMSetDepthStencilState(
					D3D11->DeviceContext,
					D3D11->DepthStencilState, 
					0
				);
				D3D11->DeviceContext->lpVtbl->OMSetBlendState(
					D3D11->DeviceContext,
					Null, Null,
					0xFFFFFFFF
				);
				D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, TotalVertices, VertexOffset);
				VertexOffset += TotalVertices;
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_GLYPH)
			{
				D3D11->DeviceContext->lpVtbl->IASetPrimitiveTopology(D3D11->DeviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				D3D11->DeviceContext->lpVtbl->IASetInputLayout(D3D11->DeviceContext, D3D11->TriangleInputLayout);
				u32 Stride = sizeof(gpu_vertex);
				u32 Offset = 0;
				D3D11->DeviceContext->lpVtbl->IASetVertexBuffers(D3D11->DeviceContext, 0, 1, &D3D11->VertexBuffers[ResourceIndex], &Stride, &Offset);
				D3D11->DeviceContext->lpVtbl->VSSetShader(
					D3D11->DeviceContext,
					D3D11->GlyphVertexShader,
					Null, 
					0
				);
				D3D11->DeviceContext->lpVtbl->VSSetConstantBuffers(
					D3D11->DeviceContext,
					0, 1,
					&D3D11->UniformBuffers[ResourceIndex]
				);
				D3D11->DeviceContext->lpVtbl->RSSetViewports(
					D3D11->DeviceContext,
					1, &viewport
				);

				D3D11_RECT ScissorRect =
				{
					Command->Scissor.X,
					Command->Scissor.Y,
					Command->Scissor.X + Command->Scissor.Z,
					Command->Scissor.Y + Command->Scissor.W,
				};
				D3D11->DeviceContext->lpVtbl->RSSetScissorRects(
					D3D11->DeviceContext,
					1,
					&ScissorRect
				);
				D3D11->DeviceContext->lpVtbl->RSSetState(
					D3D11->DeviceContext,
					D3D11->RasterizerState
				);
				D3D11->DeviceContext->lpVtbl->PSSetShader(
					D3D11->DeviceContext,
					D3D11->GlyphPixelShader,
					Null, 0
				);
				D3D11->DeviceContext->lpVtbl->PSSetShaderResources(
					D3D11->DeviceContext,
					0, 1, &D3D11->FontAtlasTextureView
				);
				D3D11->DeviceContext->lpVtbl->PSSetSamplers(
					D3D11->DeviceContext,
					0, 1, &D3D11->SamplerState
				);
				D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
					D3D11->DeviceContext, 
					1, &RenderTargetView, D3D11->DepthStencilView
				);
				D3D11->DeviceContext->lpVtbl->OMSetDepthStencilState(
					D3D11->DeviceContext,
					D3D11->DepthStencilState, 
					0
				);
				D3D11->DeviceContext->lpVtbl->OMSetBlendState(
					D3D11->DeviceContext,
					D3D11->BlendState, Null,
					0xFFFFFFFF
				);
				D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, TotalVertices, VertexOffset);
				VertexOffset += TotalVertices;
				TotalVertices = 0;
			}

			UniformCount += 1;
		}
	}

	SwapChain->lpVtbl->Present(SwapChain, 1, 0);

	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;

	R->TransformCount = 0;
	R->ScissorCount = 0;

	ScratchEnd(Scratch);
}