#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "render.h"


typedef struct d3d11_shader
{
	int TODO;
} d3d11_shader;

#define FRAMES_IN_FLIGHT (3)

typedef struct d3d11_context
{
	ID3D11Device        *Device;
	ID3D11DeviceContext *DeviceContext;

	d3d11_shader  BloomShader;
	d3d11_shader  TriangleShader;

	ID3D11Texture2D     *FontAtlasTexture;
	ID3D11Texture2D     *DepthTexture;

	ID3D11Buffer *TriangleVertexBuffers[FRAMES_IN_FLIGHT];
	ID3D11Buffer *GlyphVertexBuffers[FRAMES_IN_FLIGHT];
	ID3D11Buffer *UniformBuffers[FRAMES_IN_FLIGHT];
} d3d11_context;