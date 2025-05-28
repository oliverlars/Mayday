struct vertex_input
{
    float4 Position: POS;
    float4 Colour:   COL;
	float4 UV:       TEX;
	float  PointSize;
};

struct vertex_output
{
	float4 Position: SV_POSITION;
	float4 Colour:  COL;
};

cbuffer uniforms : register(b0)
{
	row_major float4x4 PVMMatrix;
}


vertex_output VertexMain(vertex_input Vertex)
{
	vertex_output Output;
	Output.Position = mul(Vertex.Position, PVMMatrix);
	Output.Colour = Vertex.Colour;

	return Output;
}

float4 PixelMain(vertex_output Input)
{
	float4 Colour = Input.Colour;
	return Colour;
}