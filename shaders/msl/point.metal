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
	float2 Centre    [[attribute(3)]];
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
	Output.Centre = Vertices[VertexID].Centre;
	return Output;
}

float DistanceToCircle(float2 Position, float2 Centre, float Radius)
{
	return length(Position - Centre) - Radius; 
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	float HalfPointSize = 0.5*Input.PointSize;
	float Distance = DistanceToCircle(Input.Position.xy, Input.Centre, HalfPointSize);
	float AA = smoothstep(HalfPointSize + HalfPointSize, HalfPointSize - HalfPointSize, Distance);
	return float4(Input.Colour.rgb, AA*Input.Colour.a);
}