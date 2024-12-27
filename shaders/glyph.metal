#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float4 Colour;
	float2 UV;
};

struct vertex_input
{
	float4 Position [[attribute(0)]];
	float4 Colour   [[attribute(1)]];
	float4 UV       [[attribute(2)]];
};

struct uniforms
{
	float4x4 PVMMatrix;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	Output.UV = Vertices[VertexID].UV.xy;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]],
							 texture2d<float> FontAtlas [[texture(0)]],
							 sampler Sampler [[sampler(0)]])
{
	float4 Colour = float4(Input.Colour.rgb, FontAtlas.sample(Sampler, Input.UV).r);
	return Colour;
}