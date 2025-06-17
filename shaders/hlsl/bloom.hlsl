Texture2D    Base: register(t0);
Texture2D    Bloom: register(t1);
SamplerState Sampler:   register(s0);


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

float4 ToSRGB(float4 LinearRGB)
{
	float3 Higher = float3(1.055, 1.055, 1.055)*pow(LinearRGB.rgb, float3(1.0/2.4, 1.0/2.4, 1.0/2.4)) - float3(0.055, 0.055, 0.055);
	float3 Lower = LinearRGB.rgb * float3(12.92, 12.92, 12.92);

	float3 RGB = float3(0.0, 0.0, 0.0);
	if(all(LinearRGB.rgb < float3(0.0031308, 0.0031308, 0.0031308)))
	{
		RGB = Lower;
	}
	else
	{
		RGB = Higher;
	}

	return float4(RGB, LinearRGB.a);
}

float3 TonemapToACESFilm(float3 X)
{
	float A = 2.51;
	float B = 0.03;
	float C = 2.43;
	float D = 0.59;
	float E = 0.14;

	return clamp((X * (A*X + B)) / (X * (C*X + D) + E), 0.0, 1.0);
}

vertex_output VertexMain(uint VertexID: SV_VertexID)
{
	vertex_output Output;
	Output.Position = float4(ClipXY[VertexID], 0.0, 1.0);
	Output.UV = UV[VertexID];
	return Output;
}


float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float4 Colour = float4(Base.Sample(Sampler, Input.UV).rgb + Bloom.Sample(Sampler, Input.UV).rgb*0.7, 1.0);
	Colour.rgb = TonemapToACESFilm(Colour.rgb);
	Colour = ToSRGB(Colour);
	return Colour;
}
