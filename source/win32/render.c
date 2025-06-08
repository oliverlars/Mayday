#include <assert.h>
#include <d3d11.h>
#include <dxgiformat.h>

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
		UniformBufferDescription.ByteWidth         = sizeof(uniforms) + 15 & 0xfffffff0;
		UniformBufferDescription.Usage             = D3D11_USAGE_DYNAMIC;
		UniformBufferDescription.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
		UniformBufferDescription.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&UniformBufferDescription,
											NULL,
											&D3D11->UniformBuffers[ResourceIndex]);

		D3D11_BUFFER_DESC TriangleVertexBufferDescription = {0};
		TriangleVertexBufferDescription.ByteWidth = MegaBytes(64);
		TriangleVertexBufferDescription.Usage     = D3D11_USAGE_DYNAMIC;
		TriangleVertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		TriangleVertexBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&TriangleVertexBufferDescription,
											NULL,
											&D3D11->VertexBuffers[ResourceIndex]);
	}


	// TRIANGLE SHADER
	{
		ID3DBlob *TriangleVertexShaderObject;
		ID3DBlob *Error;
		HRESULT Compiled;
		Compiled = D3DCompile(G_TriangleShader, sizeof(G_TriangleShader),
				Null, Null, Null,
				"VertexMain",
				"vs_5_0",
				0, 0,
				&TriangleVertexShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[VERTEX] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}
		
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
		Compiled = D3DCompile(G_TriangleShader, sizeof(G_TriangleShader),
				Null, Null, Null,
				"PixelMain",
				"ps_5_0",
				0, 0,
				&TrianglePixelShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[PIXEL] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}

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

	// Gaussian Shader
	{
		ID3DBlob *GaussianVertexShaderObject;
		ID3DBlob *Error;
		HRESULT Compiled;

		Compiled = D3DCompile(G_GaussianShader, sizeof(G_GaussianShader),
				Null, Null, Null,
				"VertexMain",
				"vs_5_0",
				0, 0,
				&GaussianVertexShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[VERTEX] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}
		
		D3D11->Device->lpVtbl->CreateVertexShader(
			D3D11->Device,
			GaussianVertexShaderObject->lpVtbl->GetBufferPointer(GaussianVertexShaderObject),
			GaussianVertexShaderObject->lpVtbl->GetBufferSize(GaussianVertexShaderObject),
			Null, &D3D11->GaussianVertexShader
		);

		ID3DBlob *HorizontalGaussianPixelShaderObject;
		Compiled = D3DCompile(G_GaussianShader, sizeof(G_GaussianShader),
				Null, Null, Null,
				"BlurHorizontal",
				"ps_5_0",
				0, 0,
				&HorizontalGaussianPixelShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[PIXEL] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}

		D3D11->Device->lpVtbl->CreatePixelShader(
			D3D11->Device,
			HorizontalGaussianPixelShaderObject->lpVtbl->GetBufferPointer(HorizontalGaussianPixelShaderObject),
			HorizontalGaussianPixelShaderObject->lpVtbl->GetBufferSize(HorizontalGaussianPixelShaderObject),
			Null, &D3D11->HorizontalGaussianPixelShader
		);

		ID3DBlob *VerticalGaussianPixelShaderObject;
		Compiled = D3DCompile(G_GaussianShader, sizeof(G_GaussianShader),
				Null, Null, Null,
				"BlurVertical",
				"ps_5_0",
				0, 0,
				&VerticalGaussianPixelShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[PIXEL] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}

		D3D11->Device->lpVtbl->CreatePixelShader(
			D3D11->Device,
			VerticalGaussianPixelShaderObject->lpVtbl->GetBufferPointer(VerticalGaussianPixelShaderObject),
			VerticalGaussianPixelShaderObject->lpVtbl->GetBufferSize(VerticalGaussianPixelShaderObject),
			Null, &D3D11->VerticalGaussianPixelShader
		);
	}

	// Bloom Shader
	{
		ID3DBlob *BloomVertexShaderObject;
		ID3DBlob *Error;
		HRESULT Compiled;

		Compiled = D3DCompile(G_BloomShader, sizeof(G_BloomShader),
				Null, Null, Null,
				"VertexMain",
				"vs_5_0",
				0, 0,
				&BloomVertexShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[VERTEX] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}
		
		D3D11->Device->lpVtbl->CreateVertexShader(
			D3D11->Device,
			BloomVertexShaderObject->lpVtbl->GetBufferPointer(BloomVertexShaderObject),
			BloomVertexShaderObject->lpVtbl->GetBufferSize(BloomVertexShaderObject),
			Null, &D3D11->BloomVertexShader
		);

		ID3DBlob *BloomPixelShaderObject;
		Compiled = D3DCompile(G_BloomShader, sizeof(G_BloomShader),
				Null, Null, Null,
				"PixelMain",
				"ps_5_0",
				0, 0,
				&BloomPixelShaderObject, &Error);
		if(FAILED(Compiled))
		{
			char *ErrorMessage = (char*)(Error->lpVtbl->GetBufferPointer(Error));
			LogError("[PIXEL] ?cstr", ErrorMessage);
			Error->lpVtbl->Release(Error);
		}

		D3D11->Device->lpVtbl->CreatePixelShader(
			D3D11->Device,
			BloomPixelShaderObject->lpVtbl->GetBufferPointer(BloomPixelShaderObject),
			BloomPixelShaderObject->lpVtbl->GetBufferSize(BloomPixelShaderObject),
			Null, &D3D11->BloomPixelShader
		);
	}

	D3D11_RASTERIZER_DESC RasterizerStateDescription = {0};
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

	D3D11_SAMPLER_DESC SamplerDescription = {0};
	SamplerDescription.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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

void R_Update(render_context *R, m_arena *FrameArena)
{
	d3d11_context *D3D11 = R->Backend;
	IDXGISwapChain *SwapChain = (IDXGISwapChain*)sapp_d3d11_get_swap_chain();
	ID3D11RenderTargetView *RTV = (ID3D11RenderTargetView*)sapp_d3d11_get_render_view();
	ID3D11DepthStencilView *DSV = (ID3D11DepthStencilView*)sapp_d3d11_get_depth_stencil_view();



	ID3D11Texture2D *FrameBuffer;

	SwapChain->lpVtbl->GetBuffer(SwapChain, 0, &IID_ID3D11Texture2D, (void**)&FrameBuffer);
	D3D11_TEXTURE2D_DESC FrameBufferDescription = {0};	
	FrameBuffer->lpVtbl->GetDesc(FrameBuffer, &FrameBufferDescription);
	v2i Dimensions = v2i(FrameBufferDescription.Width, FrameBufferDescription.Height);
	D3D11_VIEWPORT Viewport =
	{
		0.0f, 0.0f, 
		(f32)FrameBufferDescription.Width,
		(f32)FrameBufferDescription.Height,
		0.0f, 1.0f
	};

	if(!D3D11->BaseFrameBuffer)
	{
		D3D11_TEXTURE2D_DESC BaseFrameBufferDescription = FrameBufferDescription;
		BaseFrameBufferDescription.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BaseFrameBufferDescription, NULL,
			&D3D11->BaseFrameBuffer);
		D3D11_RENDER_TARGET_VIEW_DESC BaseFrameBufferRTVDesc = {};
		BaseFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BaseFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->BaseFrameBuffer,
			&BaseFrameBufferRTVDesc,
			&D3D11->BaseFrameBufferRTV
		); 

		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->BaseFrameBuffer,
			Null,
			&D3D11->BaseFrameBufferResourceView
		);
	}
	else
	{
		D3D11_TEXTURE2D_DESC BaseFrameBufferDescription = {0};	
			D3D11->BaseFrameBuffer->lpVtbl->GetDesc(
			D3D11->BaseFrameBuffer,
			&BaseFrameBufferDescription
		);
		if(BaseFrameBufferDescription.Width != FrameBufferDescription.Width ||
		   BaseFrameBufferDescription.Height != FrameBufferDescription.Height)
		{
			D3D11->BaseFrameBuffer->lpVtbl->Release(
				D3D11->BaseFrameBuffer
			);

			BaseFrameBufferDescription.Width = FrameBufferDescription.Width;
			BaseFrameBufferDescription.Height = FrameBufferDescription.Height;

			D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BaseFrameBufferDescription, NULL,
			&D3D11->BaseFrameBuffer);
		}

		D3D11_RENDER_TARGET_VIEW_DESC BaseFrameBufferRTVDesc = {};
		BaseFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BaseFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->BaseFrameBufferRTV->lpVtbl->Release(
			D3D11->BaseFrameBufferRTV
		);
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->BaseFrameBuffer,
			&BaseFrameBufferRTVDesc,
			&D3D11->BaseFrameBufferRTV
		); 

		D3D11->BaseFrameBufferResourceView->lpVtbl->Release(
			D3D11->BaseFrameBufferResourceView
		);
		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->BaseFrameBuffer,
			Null,
			&D3D11->BaseFrameBufferResourceView
		);
	}

	if(!D3D11->GaussianBlurFrameBuffers[0])
	{
		D3D11_TEXTURE2D_DESC BlurredFrameBufferDescription = FrameBufferDescription;
		BlurredFrameBufferDescription.BindFlags |= D3D11_BIND_SHADER_RESOURCE;		
		D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BlurredFrameBufferDescription, NULL,
			&D3D11->GaussianBlurFrameBuffers[0]);

		D3D11_RENDER_TARGET_VIEW_DESC BlurredFrameBufferRTVDesc = {};
		BlurredFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BlurredFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[0],
			&BlurredFrameBufferRTVDesc,
			&D3D11->GaussianBlurFrameBufferRTVs[0]
		); 

		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[0],
			Null,
			&D3D11->GaussianBlurFrameBufferResourceViews[0]
		);
	}
	else
	{
		D3D11_TEXTURE2D_DESC BlurredFrameBufferDescription = {0};	
			D3D11->GaussianBlurFrameBuffers[0]->lpVtbl->GetDesc(
			D3D11->GaussianBlurFrameBuffers[0],
			&BlurredFrameBufferDescription
		);
		if(BlurredFrameBufferDescription.Width != FrameBufferDescription.Width ||
		BlurredFrameBufferDescription.Height != FrameBufferDescription.Height)
		{
			D3D11->GaussianBlurFrameBuffers[0]->lpVtbl->Release(
				D3D11->GaussianBlurFrameBuffers[0]
			);
			BlurredFrameBufferDescription.Width = FrameBufferDescription.Width;
			BlurredFrameBufferDescription.Height = FrameBufferDescription.Height;

			D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BlurredFrameBufferDescription, NULL,
			&D3D11->GaussianBlurFrameBuffers[0]);
		}

		D3D11_RENDER_TARGET_VIEW_DESC BlurredFrameBufferRTVDesc = {};
		BlurredFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BlurredFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->GaussianBlurFrameBufferRTVs[0]->lpVtbl->Release(
			D3D11->GaussianBlurFrameBufferRTVs[0]
		);
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[0],
			&BlurredFrameBufferRTVDesc,
			&D3D11->GaussianBlurFrameBufferRTVs[0]
		); 

		D3D11->GaussianBlurFrameBufferResourceViews[0]->lpVtbl->Release(
			D3D11->GaussianBlurFrameBufferResourceViews[0]
		);
		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[0],
			Null,
			&D3D11->GaussianBlurFrameBufferResourceViews[0]
		);
	}

		if(!D3D11->GaussianBlurFrameBuffers[1])
	{
		D3D11_TEXTURE2D_DESC BlurredFrameBufferDescription = FrameBufferDescription;
		BlurredFrameBufferDescription.BindFlags |= D3D11_BIND_SHADER_RESOURCE;		
		D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BlurredFrameBufferDescription, NULL,
			&D3D11->GaussianBlurFrameBuffers[1]);

		D3D11_RENDER_TARGET_VIEW_DESC BlurredFrameBufferRTVDesc = {};
		BlurredFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BlurredFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[1],
			&BlurredFrameBufferRTVDesc,
			&D3D11->GaussianBlurFrameBufferRTVs[1]
		); 

		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[1],
			Null,
			&D3D11->GaussianBlurFrameBufferResourceViews[1]
		);
	}
	else
	{
		D3D11_TEXTURE2D_DESC BlurredFrameBufferDescription = {0};	
			D3D11->GaussianBlurFrameBuffers[1]->lpVtbl->GetDesc(
			D3D11->GaussianBlurFrameBuffers[1],
			&BlurredFrameBufferDescription
		);
		if(BlurredFrameBufferDescription.Width != FrameBufferDescription.Width ||
		BlurredFrameBufferDescription.Height != FrameBufferDescription.Height)
		{
			D3D11->GaussianBlurFrameBuffers[1]->lpVtbl->Release(
				D3D11->GaussianBlurFrameBuffers[1]
			);
			BlurredFrameBufferDescription.Width = FrameBufferDescription.Width;
			BlurredFrameBufferDescription.Height = FrameBufferDescription.Height;

			D3D11->Device->lpVtbl->CreateTexture2D(
			D3D11->Device,
			&BlurredFrameBufferDescription, NULL,
			&D3D11->GaussianBlurFrameBuffers[1]);
		}

		D3D11_RENDER_TARGET_VIEW_DESC BlurredFrameBufferRTVDesc = {};
		BlurredFrameBufferRTVDesc.Format        = FrameBufferDescription.Format;
		BlurredFrameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		D3D11->GaussianBlurFrameBufferRTVs[1]->lpVtbl->Release(
			D3D11->GaussianBlurFrameBufferRTVs[1]
		);
		D3D11->Device->lpVtbl->CreateRenderTargetView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[1],
			&BlurredFrameBufferRTVDesc,
			&D3D11->GaussianBlurFrameBufferRTVs[1]
		); 

		D3D11->GaussianBlurFrameBufferResourceViews[1]->lpVtbl->Release(
			D3D11->GaussianBlurFrameBufferResourceViews[1]
		);
		D3D11->Device->lpVtbl->CreateShaderResourceView(
			D3D11->Device,
			(ID3D11Resource*)D3D11->GaussianBlurFrameBuffers[1],
			Null,
			&D3D11->GaussianBlurFrameBufferResourceViews[1]
		);
	}

	f32 ClearColour[4] = {0.0, 0.0, 0.0, 1.0};
	D3D11->DeviceContext->lpVtbl->ClearRenderTargetView(
		D3D11->DeviceContext,
		RTV,
		ClearColour
	);

	D3D11->DeviceContext->lpVtbl->ClearRenderTargetView(
		D3D11->DeviceContext,
		D3D11->BaseFrameBufferRTV,
		ClearColour
	);

	D3D11->DeviceContext->lpVtbl->ClearRenderTargetView(
		D3D11->DeviceContext,
		D3D11->GaussianBlurFrameBufferRTVs[0],
		ClearColour
	);

	D3D11->DeviceContext->lpVtbl->ClearRenderTargetView(
		D3D11->DeviceContext,
		D3D11->GaussianBlurFrameBufferRTVs[1],
		ClearColour
	);


	D3D11->DeviceContext->lpVtbl->ClearDepthStencilView(
		D3D11->DeviceContext,
		DSV,
		D3D11_CLEAR_DEPTH,
		1.0f, 0
	);

	// NOTE(Oliver): If we don't do this we can't resize 
	FrameBuffer->lpVtbl->Release(FrameBuffer);  


	m_scratch_arena Scratch = ScratchBegin(0, 0);

	
	int TotalVertices = 0;
	int VertexOffset = 0;

	int ResourceIndex = R->Frame % FRAMES_IN_FLIGHT;

	D3D11_MAP VertexWriteMode = D3D11_MAP_WRITE_DISCARD;
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
				VertexWriteMode, 0, &TriangleVertexBufferResource
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
			VertexWriteMode = D3D11_MAP_WRITE_NO_OVERWRITE;

			TotalVertices += Command->VerticesCount;
		}
		else
		{
			continue;
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
				&UniformBufferResourse);
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
				u32 Offset = VertexOffset*sizeof(gpu_vertex);
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
					1, &Viewport
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
					1, &D3D11->BaseFrameBufferRTV, Null
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
				D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, TotalVertices, 0);
				VertexOffset += TotalVertices;
				TotalVertices = 0;
			}
			else if(Command->Type == R_COMMAND_GLYPH)
			{
				D3D11->DeviceContext->lpVtbl->IASetPrimitiveTopology(D3D11->DeviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				D3D11->DeviceContext->lpVtbl->IASetInputLayout(D3D11->DeviceContext, D3D11->GlyphInputLayout);
				u32 Stride = sizeof(gpu_vertex);
				u32 Offset = VertexOffset*sizeof(gpu_vertex);
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
					1, &Viewport
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
					1, &D3D11->BaseFrameBufferRTV, Null
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
				D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, TotalVertices, 0);
				VertexOffset += TotalVertices;
				TotalVertices = 0;
			}
		}
	}


	// Do blur
	{
		D3D11->DeviceContext->lpVtbl->RSSetViewports(
			D3D11->DeviceContext,
			1, &Viewport
		);

		D3D11_RECT ScissorRect =
		{
			0,
			0,
			Dimensions.X,
			Dimensions.Y,
		};

		D3D11->DeviceContext->lpVtbl->RSSetScissorRects(
			D3D11->DeviceContext,
			1,
			&ScissorRect
		);

		D3D11->DeviceContext->lpVtbl->PSSetSamplers(
			D3D11->DeviceContext,
			0, 1, &D3D11->SamplerState
		);

		D3D11->DeviceContext->lpVtbl->RSSetState(
			D3D11->DeviceContext,
			D3D11->RasterizerState
		);

		D3D11->DeviceContext->lpVtbl->VSSetShader(
			D3D11->DeviceContext,
			D3D11->GaussianVertexShader,
			Null, 
			0
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

		D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
			D3D11->DeviceContext, 
			1, &D3D11->GaussianBlurFrameBufferRTVs[0], Null
		);

		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(
			D3D11->DeviceContext,
			0, 1, &D3D11->BaseFrameBufferResourceView
		);


		D3D11->DeviceContext->lpVtbl->PSSetShader(
			D3D11->DeviceContext,
			D3D11->HorizontalGaussianPixelShader,
			Null, 0
		);
		D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, 3, 0);

		D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
			D3D11->DeviceContext, 
			1, &RTV, Null
		);

		D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
			D3D11->DeviceContext, 
			1, &D3D11->GaussianBlurFrameBufferRTVs[1], Null
		);

		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(
			D3D11->DeviceContext,
			0, 1, &D3D11->GaussianBlurFrameBufferResourceViews[0]
		);
		D3D11->DeviceContext->lpVtbl->PSSetShader(
			D3D11->DeviceContext,
			D3D11->VerticalGaussianPixelShader,
			Null, 0
		);
		D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, 3, 0);
		

		ID3D11ShaderResourceView* SRV[1] = { Null };
		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(D3D11->DeviceContext, 0, 1, SRV);
	}

	// Do bloom
	{
		D3D11->DeviceContext->lpVtbl->RSSetViewports(
			D3D11->DeviceContext,
			1, &Viewport
		);

		D3D11_RECT ScissorRect =
		{
			0,
			0,
			Dimensions.X,
			Dimensions.Y,
		};

		D3D11->DeviceContext->lpVtbl->RSSetScissorRects(
			D3D11->DeviceContext,
			1,
			&ScissorRect
		);

		D3D11->DeviceContext->lpVtbl->PSSetSamplers(
			D3D11->DeviceContext,
			0, 1, &D3D11->SamplerState
		);

		D3D11->DeviceContext->lpVtbl->RSSetState(
			D3D11->DeviceContext,
			D3D11->RasterizerState
		);

		D3D11->DeviceContext->lpVtbl->VSSetShader(
			D3D11->DeviceContext,
			D3D11->BloomVertexShader,
			Null, 
			0
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

		D3D11->DeviceContext->lpVtbl->OMSetRenderTargets(
			D3D11->DeviceContext, 
			1, &RTV, Null
		);

		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(
			D3D11->DeviceContext,
			0, 1, &D3D11->BaseFrameBufferResourceView
		);
		
		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(
			D3D11->DeviceContext,
			1, 1, &D3D11->GaussianBlurFrameBufferResourceViews[1]
		);

		D3D11->DeviceContext->lpVtbl->PSSetShader(
			D3D11->DeviceContext,
			D3D11->BloomPixelShader,
			Null, 0
		);
		D3D11->DeviceContext->lpVtbl->Draw(D3D11->DeviceContext, 3, 0);

		ID3D11ShaderResourceView* NullSRV[1] = { Null };
		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(D3D11->DeviceContext, 0, 1, NullSRV);
		D3D11->DeviceContext->lpVtbl->PSSetShaderResources(D3D11->DeviceContext, 1, 1, NullSRV);

	}

	// I don't need to call this lol
	//SwapChain->lpVtbl->Present(SwapChain, 1, 0);

	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;

	R->TransformCount = 0;
	R->ScissorCount = 0;

	ScratchEnd(Scratch);
}