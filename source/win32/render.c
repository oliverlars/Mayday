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

void R_Update(render_context *R, m_arena *FrameArena)
{
	IDXGISwapChain *SwapChain = (IDXGISwapChain*)sapp_d3d11_get_swap_chain();
	
	ID3D11Texture2D *FrameBuffer;
	SwapChain->lpVtbl->GetBuffer(SwapChain, 0, &IID_ID3D11Texture2D, (void**)&FrameBuffer);

}