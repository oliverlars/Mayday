#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float2 UV;
};

vertex vertex_output VertexMain(uint VertexID [[vertex_id]])
{
	vertex_output Output;
	float2 Vertices[6] =
	{
		float2(-1, -1), float2(1, -1), float2(1, 1),
		float2(-1, -1), float2(-1, 1), float2(1, 1)
	};
	Output.Position = float4(Vertices[VertexID], 0, 1);
	Output.UV = Vertices[VertexID]*0.5 + 0.5;
	Output.UV.y = 1.0 - Output.UV.y;
	return Output;
}

inline float3 TonemapToACESFilm(float3 X)
{
	const float A = 2.51;
	const float B = 0.03;
	const float C = 2.43;
	const float D = 0.59;
	const float E = 0.14;

    return clamp((X * (A*X + B)) / (X * (C*X + D) + E), 0.0, 1.0);
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> Base [[texture(0)]],
							 texture2d<float> Bloom [[texture(1)]],
							 sampler Sampler [[sampler(0)]])
{
	float4 Colour = float4(Base.sample(Sampler, Input.UV).rgb + Bloom.sample(Sampler, Input.UV).rgb*0.7, 1.0);
	
	Colour.rgb = TonemapToACESFilm(Colour.rgb);
	return Colour;
}