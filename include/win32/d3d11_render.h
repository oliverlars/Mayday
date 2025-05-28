#include <dxgi.h>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "render.h"


#define FRAMES_IN_FLIGHT (3)

typedef struct d3d11_context
{
	ID3D11Device          *Device;
	ID3D11DeviceContext   *DeviceContext;

	ID3D11RasterizerState   *RasterizerState;
	ID3D11DepthStencilState *DepthStencilState;
	ID3D11DepthStencilView  *DepthStencilView;

	ID3D11VertexShader    *TriangleVertexShader;
	ID3D11PixelShader     *TrianglePixelShader;

	ID3D11InputLayout     *InputLayout;

	ID3D11Texture2D       *FontAtlasTexture;
	ID3D11Texture2D       *DepthTexture;

	ID3D11Buffer          *TriangleVertexBuffers[FRAMES_IN_FLIGHT];
	ID3D11Buffer          *GlyphVertexBuffers[FRAMES_IN_FLIGHT];
	ID3D11Buffer          *UniformBuffers[FRAMES_IN_FLIGHT];
} d3d11_context;