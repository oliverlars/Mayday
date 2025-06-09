

Texture2D    Texture: register(t0);
SamplerState Sampler:   register(s0);

cbuffer uniforms : register(b0)
{
	float Sigma;
	int   Radius;
};

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

float Gaussian(float X, float Sigma)
{
    return exp(-(X * X) / (2.0 * Sigma * Sigma));
}

float4 BlurHorizontal(vertex_output Input) : SV_TARGET
{
	float4 Colour = float4(0, 0, 0, 0);
	float TotalWeight = 0.0f;
	float2 TexelSize;
	Texture.GetDimensions(TexelSize.x, TexelSize.y);
	TexelSize = 1.0 / TexelSize;
	for(int Index = -30; Index <  30; Index++)
	{
		float Offset = float(Index);
		float2 SampleUV = Input.UV + float2(1, 0) * Offset * TexelSize;
		float Weight = Gaussian(Offset, 10.0);

		Colour += Texture.Sample(Sampler, SampleUV) * Weight;
		TotalWeight += Weight;
	}

	return Colour / TotalWeight;
}

float4 BlurVertical(vertex_output Input) : SV_TARGET
{
	float4 Colour = float4(0, 0, 0, 0);
	float TotalWeight = 0.0f;
	float2 TexelSize;
	Texture.GetDimensions(TexelSize.x, TexelSize.y);
	TexelSize = 1.0 / TexelSize;
	for(int Index = -30; Index <  30; Index++)
	{
		float Offset = float(Index);
		float2 SampleUV = Input.UV + float2(0, 1) * Offset * TexelSize;
		float Weight = Gaussian(Offset, 10.0);

		Colour += Texture.Sample(Sampler, SampleUV) * Weight;
		TotalWeight += Weight;
	}

	return Colour / TotalWeight;
}
