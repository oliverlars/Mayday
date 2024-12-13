#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float4 Colour;
};

vertex vertex_output VertexMain(uint VertexID [[vertex_id]])
{
	float2 Position[3] = { float2(0,1), float2(-1,-1), float2(1,-1) };
	float4 Colours[3] = {float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1)};
	vertex_output Output;
	Output.Position = float4(Position[VertexID], 0, 1);
	Output.Colour = Colours[VertexID];
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	return Input.Colour;
}