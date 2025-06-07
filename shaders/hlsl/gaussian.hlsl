

Texture2D    Texture: register(t0);
SamplerState Sampler:   register(s0);

cbuffer uniforms : register(b0)
{
	bool Horizontal;
	float Sigma;
}

struct vertex_output
{
	float4 Position: SV_Position;
	float2 UV: TexCoord;
};

static float2 ClipXY[3] = {
	float2(-1.0, -1.0),
	float2(-1.0,  3.0),
	float2( 3.0, -1.0)
};

vertex_output VertexMain(uint VertexID: SV_VertexID)
{
	vertex_output Output;
	Output.Position = float4(ClipXY[VertexID], 0.0, 1.0);
	Output.UV = ClipXY[VertexID] * 0.5 + 0.5;
	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	return float4(0, 1, 0, 1);//Texture.Sample(Sampler, Input.UV);
}
