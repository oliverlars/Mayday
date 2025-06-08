

Texture2D    Texture: register(t0);
SamplerState Sampler:   register(s0);

cbuffer uniforms : register(b0)
{
	bool Horizontal;
	float Sigma;
}

struct vertex_output
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD0;
};

static float2 ClipXY[3] = {
	float2(-1.0, -3.0),
	float2( 3.0,  1.0),
	float2(-1.0,  1.0),
};

static float2 UV[3] = {
	float2(0.0, 2.0),
	float2(2.0, 0.0),
	float2(0.0, 0.0),
};

vertex_output VertexMain(uint VertexID: SV_VertexID)
{
	vertex_output Output;
	Output.Position = float4(ClipXY[VertexID], 0.0, 1.0);
	Output.UV = UV[VertexID];
	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	return Texture.Sample(Sampler, Input.UV);
}
