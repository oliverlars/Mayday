#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float4 Colour;
};

struct vertex_input
{
	float4 Position;
	float4 Colour;
};

struct uniforms
{
	float4x4 PerspectiveMatrix;
	float4x4 ViewMatrix;
	float4x4 ModelMatrix;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PerspectiveMatrix * Uniforms.ViewMatrix * Uniforms.ModelMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	return Input.Colour;
}