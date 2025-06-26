#include <metal_stdlib>

using namespace metal;

struct vertex_output
{
	float4 Position [[position]];
	float4 Colour;
	float2 Start;
	float2 End;
};

struct vertex_input
{
	float4 Position [[attribute(0)]];
	float4 Colour   [[attribute(1)]];
	float2 Start    [[attribute(2)]];
	float2 End      [[attribute(3)]];
};

struct uniforms
{
	float4x4 PVMMatrix;
};

float DistanceToSegment(float2 Position, float2 Start, float2 End)
{
	float2 EndToStart = End - Start;
	float2 PositionToStart = Position - Start;
	float H = clamp( dot(PositionToStart, EndToStart)/dot(EndToStart,EndToStart), 0.0, 1.0);
	return length(PositionToStart - H*EndToStart);
}

vertex vertex_output VertexMain(constant vertex_input *Vertices [[buffer(0)]],
								constant uniforms &Uniforms [[buffer(1)]],
								uint VertexID [[vertex_id]])
{
	vertex_output Output;
	Output.Position = Uniforms.PVMMatrix * Vertices[VertexID].Position;
	Output.Colour = Vertices[VertexID].Colour;
	return Output;
}

fragment float4 FragmentMain(vertex_output Input [[stage_in]])
{
	//float Distance = DistanceToSegment(Input.Position.xy, Input.Start, Input.End);

	return float4(1, 0, 0, 1);

	// if(Distance < 0.0)
	// {
	// 	return float4(0, 1, 0, 1.0);
	// }
	// else
	// {
	// 	return float4(1, 0, 0, 1.0);
	// }

	// float4 Colour = float4(Input.Colour.rgb, 1.0);
	// return Colour;
}