struct vertex_input
{
	float4 Position:  POS;
	float4 Colour:    COL;
	float2 Start:     START;
	float2 End:       END;
	float  Thickness: THICKNESS;
};

struct vertex_output
{
	float4 Position: SV_POSITION;
	float4 Colour:  COL;

	nointerpolation float2 Start: START;
	nointerpolation float2 End: END;
	float Thickness: THICKNESS;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
};

float DistanceToSegment(float2 Position, in float2 Start, float2 End)
{
	float2 EndToStart = End - Start;
	float2 PositionToStart = Position - Start;
	float H = clamp( dot(PositionToStart, EndToStart)/dot(EndToStart,EndToStart), 0.0, 1.0);
	return length(PositionToStart - H*EndToStart);
}


vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;

	Output.Start = Vertex.Start;
	Output.End   = Vertex.End;
	Output.Thickness = Vertex.Thickness;


	return Output;
}

float4 PixelMain(vertex_output Input) : SV_TARGET
{
	float HalfThickness = 0.5 * Input.Thickness;
	float Distance = DistanceToSegment(Input.Position.xy, Input.Start, Input.End) - HalfThickness;
	float AA = smoothstep(HalfThickness + HalfThickness, HalfThickness - HalfThickness, Distance);


	return float4(1, 0, 0, 1);
	return float4(Input.Colour.rgb, float(AA));

}