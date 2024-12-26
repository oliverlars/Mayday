#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float PointSize [[point_size]];
	float4 Colour;
};

struct vertex_input
{
	float4 Position [[attribute(0)]];
	float4 Colour   [[attribute(1)]];
	float4 UV;
};

struct uniforms
{
	float4x4 ProjectionMatrix;
	float4x4 ViewMatrix [[row_major]];
	float4x4 ModelMatrix;
};

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.ProjectionMatrix * Uniforms.ViewMatrix * Uniforms.ModelMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	Output.PointSize = 10.0;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float4 Colour = float4(Input.Colour.rgb, 1.0);
	//if()
	return Colour;
}