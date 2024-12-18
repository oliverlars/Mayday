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

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> TextureA [[texture(0)]],
							 texture2d<float> TextureB [[texture(1)]],
							 sampler Sampler [[sampler(0)]])
{
	float4 Colour = float4(TextureA.sample(Sampler, Input.UV).rgb + TextureB.sample(Sampler, Input.UV).rgb, 1.0);
	return Colour;
}