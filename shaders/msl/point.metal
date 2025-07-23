#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float PointSize;
	float4 Colour;
	float2 Centre;
};

struct vertex_input
{
	float4 Position  [[attribute(0)]];
	float4 Colour    [[attribute(1)]];
	float  PointSize [[attribute(2)]];
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
	Output.PointSize = Vertices[VertexID].PointSize;
	return Output;
}

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return length(Position - Centre) - Radius; 
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float4 Colour = Input.Colour;
	float HalfPointSize = 0.5 * Input.PointSize;
	float Distance = DistanceToCircle(Input.Position.xy, float2(0, 0), Input.PointSize);
	return Colour;
}