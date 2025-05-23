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
	float  PointSize;
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

fragment float4 FragmentMain(vertex_output Input [[stage_in]], float2 PointCoordinate [[point_coord]])
{
	float4 Colour = float4(Input.Colour.rgb, 1.0);
	if(length(PointCoordinate - float2(0.5)) > 0.5)
	{
		discard_fragment();
	}
	return Colour;
}