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
	ID3D11InputLayout     *TriangleInputLayout;

	ID3D11VertexShader    *GlyphVertexShader;
	ID3D11PixelShader     *GlyphPixelShader;
	ID3D11InputLayout     *GlyphInputLayout;

	ID3D11VertexShader    *GaussianVertexShader;

	ID3D11PixelShader     *HorizontalGaussianPixelShader;
	ID3D11PixelShader     *VerticalGaussianPixelShader;

	
	ID3D11VertexShader    *BloomVertexShader;
	ID3D11PixelShader     *BloomPixelShader;

	ID3D11VertexShader    *LineVertexShader;
	ID3D11PixelShader     *LinePixelShader;
	ID3D11InputLayout     *LineInputLayout;

	ID3D11Texture2D          *FontAtlasTexture;
	ID3D11ShaderResourceView *FontAtlasTextureView;

	ID3D11BlendState      *BlendState;

	ID3D11Texture2D       *DepthTexture;

	ID3D11SamplerState    *SamplerState;

	ID3D11Texture2D       *BaseFrameBuffer;
	ID3D11Texture2D       *GaussianBlurFrameBuffers[2];

	ID3D11ShaderResourceView *BaseFrameBufferResourceView;
	ID3D11ShaderResourceView *GaussianBlurFrameBufferResourceViews[2];

	ID3D11RenderTargetView *BaseFrameBufferRTV;
	ID3D11RenderTargetView *GaussianBlurFrameBufferRTVs[2];

	ID3D11Buffer          *VertexBuffers[FRAMES_IN_FLIGHT];
	ID3D11Buffer          *UniformBuffers[FRAMES_IN_FLIGHT];
} d3d11_context;