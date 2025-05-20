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
		TriangleVertexBufferDescription.ByteWidth = MegaBytes(128);
		TriangleVertexBufferDescription.Usage     = D3D11_USAGE_DYNAMIC;
		TriangleVertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&TriangleVertexBufferDescription,
											NULL,
											NULL);

		D3D11_BUFFER_DESC GlyphVertexBufferDescription = {0};
		GlyphVertexBufferDescription.ByteWidth = MegaBytes(128);
		GlyphVertexBufferDescription.Usage     = D3D11_USAGE_DYNAMIC;
		GlyphVertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11->Device->lpVtbl->CreateBuffer(D3D11->Device,
											&GlyphVertexBufferDescription,
											NULL,
											NULL);
	}
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

	if(D3D11->DepthTexture == Null)
	{
		D3D11_TEXTURE2D_DESC DepthTextureDescription = FrameBufferDescription;
		DepthTextureDescription.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthTextureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		D3D11->Device->lpVtbl->CreateTexture2D(D3D11->Device,
											   &DepthTextureDescription,
											   Null,
											   &D3D11->DepthTexture);
	}
	else
	{
		ResizeTexture(R, D3D11->DepthTexture, Dimensions);
	}


	m_scratch_arena Scratch = ScratchBegin(0, 0);

	int UniformCount = 0;
	for(r_command *Command = R->CommandsFirst; Command != Null; Command = Command->Next)
	{
		if(0){}
		else if(Command->Type == R_COMMAND_GLYPH)
		{

		}
		else if(Command->Type == R_COMMAND_TRIANGLE)
		{

		}
		else if(Command->Type == R_COMMAND_LINES)
		{

		}

		if((Command->Next && Command->Next->Type != Command->Type) ||
		   (Command->Next && !MemoryEqual(&Command->Transform, &Command->Next->Transform, sizeof(m44f))) ||
		   (!Command->Next))
		{
			UniformCount += 1;
		}
	}

	R->CommandsFirst = Null;
	R->CommandsLast = Null;

	R->Frame += 1;

	R->TransformCount = 0;
	R->ScissorCount = 0;
}